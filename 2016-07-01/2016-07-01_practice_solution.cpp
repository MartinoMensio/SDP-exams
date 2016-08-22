#define DEBUG 1

#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif // !UNICODE

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif // !_CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>
#include <assert.h>
#include <time.h>

#define SLEEP_MIN_SEC 1
#define SLEEP_MAX_SEC 10

typedef struct _PROTECTED_RAND {
	CRITICAL_SECTION me;
	volatile INT seed;
} PROTECTED_RAND, *LPPROTECTED_RAND;
BOOL protectedRandInit();
INT protectedRand();

typedef struct _PROTECTED_INDEXES {
	CRITICAL_SECTION me;
	volatile INT count;
	LPINT indexes;
} PROTECTED_INDEXES, *LPPROTECTED_INDEXES;
BOOL protectedIndexesInit(LPPROTECTED_INDEXES, INT);

typedef struct _PROTECTED_INT {
	CRITICAL_SECTION me;
	volatile INT value;
} PROTECTED_INT, *LPPROTECTED_INT;
BOOL protectedIntInit(LPPROTECTED_INT, INT);
INT protectedIntDecrease(LPPROTECTED_INT);

typedef struct _RECORD {
	INT32 n;
	LPTSTR content;
} RECORD, *LPRECORD;
BOOL recordRead(HANDLE, LPRECORD);
BOOL recordWrite(HANDLE, LPRECORD);
BOOL recordDestroy(LPRECORD r);

typedef struct _RECORD_LIST_NODE {
	LPRECORD r;
	_RECORD_LIST_NODE *next;
} RECORD_LIST_NODE, *LPRECORD_LIST_NODE;

typedef struct _QUEUE {
	CRITICAL_SECTION me;
	CONDITION_VARIABLE empty;
	volatile LPRECORD_LIST_NODE list;
	volatile BOOL closed; // if true, there won't be any new records
} QUEUE, *LPQUEUE;
BOOL queueInit(LPQUEUE);
BOOL queueInsert(LPQUEUE, LPRECORD);
LPRECORD queueRemove(LPQUEUE); // returns NULL when the queue is empty and closed
BOOL queueClose(LPQUEUE);

typedef struct _FILEMANAGER {
	CRITICAL_SECTION me;
	HANDLE hFile;
	volatile BOOL ended;
} FILEMANAGER, *LPFILEMANAGER;
BOOL fileManagerInit(LPFILEMANAGER, HANDLE);
LPRECORD fileManagerRead(LPFILEMANAGER);
BOOL fileManagerWrite(LPFILEMANAGER, LPRECORD);
BOOL fileManagerEnd(LPFILEMANAGER);

// Global variables
INT N;
LPFILEMANAGER FilesI, FilesO;
LPQUEUE QueuesA, QueuesB;
PROTECTED_INDEXES files_active, queuesA_active, queuesB_active;
PROTECTED_INT threadsA_count, threadsB_count;
PROTECTED_RAND rand_gen;


// Prototypes
VOID randomWait(VOID);
LPRECORD randomFileRead();
//BOOL protectedIndexesDecreaseAndSignal(LPPROTECTED_INDEXES, LPQUEUE);
DWORD WINAPI threadWork(LPVOID);
VOID manipulateRecord(LPRECORD, CHAR);
BOOL randomQueueWrite(LPQUEUE, LPRECORD);
LPRECORD randomQueueRead(LPQUEUE, LPPROTECTED_INDEXES);
BOOL randomFileWrite(LPFILEMANAGER, LPRECORD);

INT _tmain(INT argc, LPCTSTR argv[]) {
	INT i;
	LPHANDLE threadsA, threadsB, threadsC;
	HANDLE hFile;
	TCHAR fileName[MAX_PATH];

	if (argc != 2) {
		_ftprintf(stderr, _T("Missing parameter. Usage: %s <N>\n"), argv[0]);
		return 1;
	}

	N = _tstoi(argv[1]);
	if (N == 0) {
		_ftprintf(stderr, _T("Parameter N must be a positive integer\n"));
		return 2;
	}

	threadsA = (LPHANDLE)calloc(N, sizeof(HANDLE));
	threadsB = (LPHANDLE)calloc(N, sizeof(HANDLE));
	threadsC = (LPHANDLE)calloc(N, sizeof(HANDLE));
	assert(threadsA);
	assert(threadsB);
	assert(threadsC);

	FilesI = (LPFILEMANAGER)calloc(N, sizeof(FILEMANAGER));
	FilesO = (LPFILEMANAGER)calloc(N, sizeof(FILEMANAGER));
	assert(FilesI);
	assert(FilesO);

	QueuesA = (LPQUEUE)calloc(N, sizeof(QUEUE));
	QueuesB = (LPQUEUE)calloc(N, sizeof(QUEUE));
	assert(QueuesA);
	assert(QueuesB);

	for (i = 0; i < N; i++) {
		_stprintf(fileName, _T("FileI%d"), i + 1);
		hFile = CreateFile(fileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		assert(hFile);
		assert(fileManagerInit(&FilesI[i], hFile));
		_stprintf(fileName, _T("FileO%d"), i + 1);
		hFile = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		assert(hFile);
		assert(fileManagerInit(&FilesO[i], hFile));
		assert(queueInit(&QueuesA[i]));
		assert(queueInit(&QueuesB[i]));
	}

	assert(protectedIndexesInit(&files_active, N));
	assert(protectedIndexesInit(&queuesA_active, N));
	assert(protectedIndexesInit(&queuesB_active, N));
	assert(protectedIntInit(&threadsA_count, N));
	assert(protectedIntInit(&threadsB_count, N));

	protectedRandInit();

	for (i = 0; i < N; i++) {
		threadsA[i] = CreateThread(NULL, 0, threadWork, (LPVOID)'a', 0, NULL);
		assert(threadsA[i]);
		threadsB[i] = CreateThread(NULL, 0, threadWork, (LPVOID)'b', 0, NULL);
		assert(threadsB[i]);
		threadsC[i] = CreateThread(NULL, 0, threadWork, (LPVOID)'c', 0, NULL);
		assert(threadsC[i]);
	}

	assert(WaitForMultipleObjects(N, threadsA, TRUE, INFINITE) == WAIT_OBJECT_0);
	assert(WaitForMultipleObjects(N, threadsB, TRUE, INFINITE) == WAIT_OBJECT_0);
	assert(WaitForMultipleObjects(N, threadsC, TRUE, INFINITE) == WAIT_OBJECT_0);

	return 0;
}

DWORD WINAPI threadWork(LPVOID param) {
	CHAR type = (CHAR)param;
	LPRECORD r;
	INT i;
	while (TRUE) {
		switch (type) {
		case 'a':
#if DEBUG > 1
			_tprintf(_T("thread type a going to wait\n"));
#endif // DEBUG
			randomWait();
#if DEBUG > 1
			_tprintf(_T("thread type a going to read\n"));
#endif // DEBUG
			r = randomFileRead();
			if (r == NULL) {
				// end of all files, this thread is going to terminate and decreases count of threadsA
				if (protectedIntDecrease(&threadsA_count) == 0) {
#if DEBUG
					_tprintf(_T("last thread type a\n"));
#endif // DEBUG
					// last threadA closes all the queuesA
					for (i = 0; i < N; i++) {
						queueClose(&QueuesA[i]);
					}
				}
				return 0;
			}
#if DEBUG
			_tprintf(_T("thread type a read: %d %s\n"), r->n, r->content);
#endif // DEBUG
			manipulateRecord(r, 'a');
#if DEBUG
			_tprintf(_T("thread type a manipulated: %d %s\n"), r->n, r->content);
#endif // DEBUG
			randomWait();
#if DEBUG > 1
			_tprintf(_T("thread type a going to write on queue a\n"));
#endif // DEBUG
			randomQueueWrite(QueuesA, r);
			break;
		case 'b':
#if DEBUG > 1
			_tprintf(_T("thread type b going to read from queue a\n"));
#endif // DEBUG
			r = randomQueueRead(QueuesA, &queuesA_active);
			if (r == NULL) {
				// end of all queuesA, this thread is going to terminate and decreases count of threadsB
				if (protectedIntDecrease(&threadsB_count) == 0) {
#if DEBUG
					_tprintf(_T("last thread type b\n"));
#endif // DEBUG
					// last threadB closes all the queuesB
					for (i = 0; i < N; i++) {
						queueClose(&QueuesB[i]);
					}
				}
				return 0;
			}
#if DEBUG
			_tprintf(_T("thread type b read: %d %s\n"), r->n, r->content);
#endif // DEBUG
			manipulateRecord(r, 'b');
#if DEBUG
			_tprintf(_T("thread type b manipulated: %d %s\n"), r->n, r->content);
#endif // DEBUG
			randomWait();
#if DEBUG > 1
			_tprintf(_T("thread type b going to write on queue b\n"));
#endif // DEBUG
			randomQueueWrite(QueuesB, r);
			break;
		case 'c':
			r = randomQueueRead(QueuesB, &queuesA_active);
			if (r == NULL) {
#if DEBUG
				_tprintf(_T("thread c terminating\n"));
#endif // DEBUG
				// end of all queuesB, this thread is going to terminate
				return 0;
			}
#if DEBUG
			_tprintf(_T("thread type c read: %d %s\n"), r->n, r->content);
#endif // DEBUG
			manipulateRecord(r, 'c');
#if DEBUG
			_tprintf(_T("thread type c manipulated: %d %s\n"), r->n, r->content);
#endif // DEBUG
			randomWait();
			randomFileWrite(FilesO, r);
			recordDestroy(r);
			break;
		default:
			break;
		}
	}
}

VOID randomWait(VOID) {
	INT s = protectedRand() % (SLEEP_MAX_SEC - SLEEP_MIN_SEC + 1) + SLEEP_MIN_SEC;
	Sleep(s * 1000);
}

BOOL fileManagerInit(LPFILEMANAGER fm, HANDLE h) {
	assert(fm);
	InitializeCriticalSection(&fm->me);
	fm->hFile = h;
	fm->ended = FALSE;
	return TRUE;
}

LPRECORD fileManagerRead(LPFILEMANAGER fm) {
	LPRECORD ret;
	EnterCriticalSection(&fm->me);
	__try {
		if (fm->ended) {
			return NULL;
		}
		ret = (LPRECORD)calloc(1, sizeof(RECORD));
		if (!recordRead(fm->hFile, ret)) {
			free(ret);
			ret = NULL;
			fm->ended = TRUE;
		}
	}
	__finally {
		LeaveCriticalSection(&fm->me);
	}
	return ret;
}

BOOL protectedIndexesInit(LPPROTECTED_INDEXES pi, INT count) {
	int i;
	InitializeCriticalSection(&pi->me);
	pi->count = count;
	pi->indexes = (LPINT)calloc(count, sizeof(INT));
	for (i = 0; i < count; i++) {
		pi->indexes[i] = i;
	}
	return TRUE;
}

BOOL protectedIntInit(LPPROTECTED_INT pi, INT count) {
	InitializeCriticalSection(&pi->me);
	pi->value = count;
	return TRUE;
}

LPRECORD randomFileRead() {
	LPRECORD r;
	INT chosenReal, chosenIndex;
	while (TRUE) {
		EnterCriticalSection(&files_active.me);
		__try {
			if (files_active.count <= 0) {
				return NULL;
			}
			chosenIndex = protectedRand() % files_active.count;
			chosenReal = files_active.indexes[chosenIndex];
		}
		__finally {
			LeaveCriticalSection(&files_active.me);
		}
#if DEBUG
		_tprintf(_T("thread type a going to read from file %d\n"), chosenIndex + 1);
#endif // DEBUG

		// the read is done exiting the critical section, so more reads can be executed in parallel
		// this can cause that more threads perform a read on the same ended file, but will happen
		// only at the end. The threads will try on another file
		r = fileManagerRead(&FilesI[chosenReal]);
		if (r == NULL) {
			// this file is at the end
			fileManagerEnd(&FilesI[chosenReal]);
			// make the fileManager unreachable from other draws
			EnterCriticalSection(&files_active.me);
			__try {
				files_active.indexes[chosenIndex] = files_active.indexes[--files_active.count];
			}
			__finally {
				LeaveCriticalSection(&files_active.me);
			}
			// continue iterating
		} else {
			return r;
		}
	}
}

BOOL recordRead(HANDLE h, LPRECORD r) {
	DWORD nRead;
	if (!ReadFile(h, &r->n, sizeof(INT32), &nRead, NULL)) {
		// read error
		_ftprintf(stderr, _T("Error reading file: %x\n"), GetLastError());
		return FALSE;
	} else if (nRead == 0) {
		// normal end of file
		return FALSE;
	} else if (nRead != sizeof(INT32)) {
		_ftprintf(stderr, _T("Read size mismatch\n"));
		return FALSE;
	}
	r->content = (LPTSTR)calloc(r->n + 1, sizeof(TCHAR));
	if (!ReadFile(h, r->content, r->n * sizeof(TCHAR), &nRead, NULL) || nRead != r->n * sizeof(TCHAR)) {
		_ftprintf(stderr, _T("Read size mismatch\n"));
		free(r->content);
		return FALSE;
	}
	// extra (already done by calloc)
	r->content[r->n] = _T('\0');
	return TRUE;
}

BOOL recordWrite(HANDLE h, LPRECORD r) {
	DWORD nWritten;
	if (!WriteFile(h, r, sizeof(INT32), &nWritten, NULL)) {
		// read error
		_ftprintf(stderr, _T("Error writing file: %x\n"), GetLastError());
		return FALSE;
	} else if (nWritten == 0) {
		// normal end of file
		return FALSE;
	} else if (nWritten != sizeof(INT32)) {
		_ftprintf(stderr, _T("Write size mismatch\n"));
		return FALSE;
	}
	if (!WriteFile(h, r->content, r->n * sizeof(TCHAR), &nWritten, NULL) || nWritten != r->n * sizeof(TCHAR)) {
		_ftprintf(stderr, _T("Read size mismatch\n"));
		return FALSE;
	}
	return TRUE;
}

BOOL recordDestroy(LPRECORD r) {
	free(r->content);
	free(r);
	return TRUE;
}

int tchar_compare(CONST VOID *a, CONST VOID *b) {
	return *((TCHAR *)a) - *((TCHAR *)b);
}

VOID manipulateRecord(LPRECORD r, CHAR mode) {
	int i, j;
	j = 0;
	LPTSTR newString;
	switch (mode) {
	case 'a':
		newString = (LPTSTR)calloc(r->n + 1, sizeof(TCHAR));
		for (i = 0; i < r->n; i++) {
			if (_istalpha(r->content[i])) {
				newString[j++] = r->content[i];
			}
		}
		free(r->content);
		r->content = newString;
		r->n = j;
		break;
	case 'b':
		for (i = 0; i < r->n; i++) {
			r->content[i] = _totupper(r->content[i]);
		}
		break;
	case 'c':
		qsort(r->content, r->n, sizeof(TCHAR), tchar_compare);
		break;
	default:
		_ftprintf(stderr, _T("Unknown mode %c\n"), mode);
		break;
	}
}

BOOL randomQueueWrite(LPQUEUE queue_set, LPRECORD r) {
	INT chosen;
	chosen = protectedRand() % N;
#if DEBUG
	_tprintf(_T("thread going to write to queue %d record %d %s\n"), chosen + 1, r->n, r->content);
#endif // DEBUG
	return queueInsert(&queue_set[chosen], r);
}

LPRECORD randomQueueRead(LPQUEUE queue_set, LPPROTECTED_INDEXES pi) {
	// TODO
	LPRECORD r;
	INT chosenReal, chosenIndex;
	while (TRUE) {
		EnterCriticalSection(&pi->me);
		__try {
			if (pi->count <= 0) {
				return NULL;
			}
			chosenIndex = protectedRand() % pi->count;
			chosenReal = pi->indexes[chosenIndex];
		}
		__finally {
			LeaveCriticalSection(&pi->me);
		}
#if DEBUG
		_tprintf(_T("thread going to read from queue %d\n"), chosenIndex + 1);
#endif // DEBUG
		r = queueRemove(&queue_set[chosenReal]);
		if (r == NULL) {
			// this queue is at the end
			// make the queue unreachable from other draws
			EnterCriticalSection(&pi->me);
			__try {
				pi->indexes[chosenIndex] = pi->indexes[--pi->count];
			}
			__finally {
				LeaveCriticalSection(&pi->me);
			}
			// continue iterating
		} else {
			return r;
		}
	}
}

// return new value
INT protectedIntDecrease(LPPROTECTED_INT pi) {
	INT ret;
	EnterCriticalSection(&pi->me);
	__try {
		ret = --pi->value;
	}
	__finally {
		LeaveCriticalSection(&pi->me);
	}
	return ret;
}

BOOL queueInit(LPQUEUE q) {
	InitializeCriticalSection(&q->me);
	InitializeConditionVariable(&q->empty);
	q->closed = FALSE;
	q->list = NULL;
	return TRUE;
}

BOOL queueInsert(LPQUEUE q, LPRECORD r) {
	LPRECORD_LIST_NODE curr;
	curr = (LPRECORD_LIST_NODE)calloc(1, sizeof(RECORD_LIST_NODE));
	curr->r = r;
	EnterCriticalSection(&q->me);
	__try {
		// insertion in head
		curr->next = q->list;
		q->list = curr;
		// wakeup
		WakeAllConditionVariable(&q->empty);
	}
	__finally {
		LeaveCriticalSection(&q->me);
	}
	return TRUE;
}

LPRECORD queueRemove(LPQUEUE q) {
	LPRECORD ret;
	volatile LPRECORD_LIST_NODE curr;
	LPRECORD_LIST_NODE previous;
	EnterCriticalSection(&q->me);
	__try {
		while (q->list == NULL && !q->closed) {
			// it's empty not closed
			SleepConditionVariableCS(&q->empty, &q->me, INFINITE);
		}
		if (q->list == NULL) {
			// is closed and empty
			return NULL;
		}
		curr = q->list;
		// it's not empty
		if (curr->next == NULL) {
			// last node
			ret = curr->r;
			q->list = NULL;
		} else {
			// at least 2 nodes
			do {
				// go ahead in the list till the last node
				previous = curr;
				curr = curr->next;
			} while (curr->next != NULL);
			assert(curr->next == NULL);
			ret = curr->r;
			previous->next = curr->next;
		}
		free(curr);
	}
	__finally {
		LeaveCriticalSection(&q->me);
	}
	return ret;
}

BOOL queueClose(LPQUEUE q) {
	EnterCriticalSection(&q->me);
	__try {
		q->closed = TRUE;
		WakeAllConditionVariable(&q->empty);
	}
	__finally {
		LeaveCriticalSection(&q->me);
	}
	return TRUE;
}

BOOL fileManagerEnd(LPFILEMANAGER fm) {
	EnterCriticalSection(&fm->me);
	__try {
		fm->ended = TRUE;
	}
	__finally {
		LeaveCriticalSection(&fm->me);
	}
	return TRUE;
}

BOOL randomFileWrite(LPFILEMANAGER fm, LPRECORD r) {
	INT chosen;
	chosen = protectedRand() % N;
#if DEBUG
	_tprintf(_T("thread type c going to write to file %d\n"), chosen + 1);
#endif // DEBUG
	return fileManagerWrite(&fm[chosen], r);
}

BOOL fileManagerWrite(LPFILEMANAGER fm, LPRECORD r) {
	BOOL ret;
	EnterCriticalSection(&fm->me);
	__try {
		ret = recordWrite(fm->hFile, r);
	}
	__finally {
		LeaveCriticalSection(&fm->me);
	}
	
	return ret;
}

BOOL protectedRandInit() {
	InitializeCriticalSection(&rand_gen.me);
	rand_gen.seed = time(NULL);
	return TRUE;
}
INT protectedRand() {
	INT ret;
	EnterCriticalSection(&rand_gen.me);
	__try {
		srand((unsigned)rand_gen.seed);
		ret = rand_gen.seed = rand();
	}
	__finally {
		LeaveCriticalSection(&rand_gen.me);
	}
	return ret;
}