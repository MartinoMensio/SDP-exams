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

#define MAX_WORD_LEN 30

typedef struct _INPUTNAMES {
	CRITICAL_SECTION me; // mutual exclusion
	TCHAR path[MAX_PATH]; // path to search
	HANDLE hFindFile; // NULL if not yet called FindFirstFile
	BOOL finished; // if true, finished the search
} INPUTNAMES, *LPINPUTNAMES;
BOOL InputNamesInit(LPINPUTNAMES, LPCTSTR path);
BOOL InputNamesRead(LPINPUTNAMES, LPTSTR inputName);

typedef struct _TRANSLATION {
	TCHAR value[MAX_WORD_LEN + 1];
	struct _TRANSLATION *next;
} TRANSLATION, *LPTRANSLATION;
typedef struct _TERM {
	TCHAR key[MAX_WORD_LEN + 1];
	INT nTranslations;
	LPTRANSLATION translation_list;
} TERM, *LPTERM;
typedef struct _TERM_NODE {
	TERM term;
	struct _TERM_NODE *next;
} TERM_NODE, *LPTERM_NODE;
typedef struct _DICTIONARY {
	INT nTerms;
	LPTERM_NODE terms;
} DICTIONARY, *LPDICTIONARY;
BOOL DictionaryInit(LPDICTIONARY);
BOOL DictionaryInsert(LPDICTIONARY, LPCTSTR key, LPCTSTR value);
BOOL DictionaryBinaryWrite(LPDICTIONARY, HANDLE hFile);

// single consumer, multiple producers
typedef struct _HANDTOHAND {
	HANDLE hRequest;
	HANDLE hResponse;
	TCHAR fileName[MAX_PATH];
	BOOL closed;
} HANDTOHAND, *LPHANDTOHAND;
BOOL HandToHandInit(LPHANDTOHAND);
BOOL HandToHandAsk(LPHANDTOHAND, LPTSTR fileName);
BOOL HandToHandDeliver(LPHANDTOHAND, LPCTSTR fileName);
BOOL HandToHandClose(LPHANDTOHAND hth);

// global variables
LPTSTR sourceDirectory, destinationDirectory;
INPUTNAMES inputNames;
HANDTOHAND intermediateFileName;

// prototypes
DWORD WINAPI SortingThread_f(LPVOID);
DWORD WINAPI MergingThread_f(LPVOID);

#define TYPE_FILE 1
#define TYPE_DIR 2
#define TYPE_DOT 3
static DWORD FileType(LPWIN32_FIND_DATA pFileData);

INT _tmain(INT argc, LPTSTR argv[]) {
	int n;
	LPTSTR fileName;
	LPHANDLE hThreads;
	int i;

	if (argc != 5) {
		_ftprintf(stderr, _T("Usage: %s n sourceDirectory destinationDirectory fileName\n"), argv[0]);
		return 1;
	}
	n = _ttoi(argv[1]);
	if (n <= 0) {
		_ftprintf(stderr, _T("n must be an positive integer value\n"));
		return 1;
	}
	sourceDirectory = argv[2];
	destinationDirectory = argv[3];
	fileName = argv[4];

	hThreads = (LPHANDLE)calloc(n + 1, sizeof(HANDLE));
	assert(hThreads);

	assert(InputNamesInit(&inputNames, sourceDirectory));
	assert(HandToHandInit(&intermediateFileName));

	for (i = 0; i < n; i++) {
		assert(hThreads[i] = CreateThread(NULL, 0, SortingThread_f, NULL, 0, NULL));
	}
	assert(hThreads[n] = CreateThread(NULL, 0, MergingThread_f, fileName, 0, NULL));
	
	assert(WaitForMultipleObjects(n, hThreads, TRUE, INFINITE) == WAIT_OBJECT_0);

	HandToHandClose(&intermediateFileName);
	assert(WaitForSingleObject(hThreads[n], INFINITE) == WAIT_OBJECT_0);

	return 0;
}

DWORD WINAPI SortingThread_f(LPVOID p) {
	DICTIONARY dictionary;
	HANDLE hInputFile, hOutputFile;
	TCHAR inputName[MAX_PATH];
	TCHAR inputPath[MAX_PATH];
	TCHAR outputPath[MAX_PATH];

	TCHAR key[MAX_WORD_LEN + 1], value[MAX_WORD_LEN + 1];
	DWORD nRead1, nRead2;

	DictionaryInit(&dictionary);

	while (InputNamesRead(&inputNames, inputName)) {
		_stprintf(inputPath, _T("%s\\%s"), sourceDirectory, inputName);
		_stprintf(outputPath, _T("%s\\%s"), destinationDirectory, inputName);

		hInputFile = CreateFile(inputPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		assert(hInputFile != INVALID_HANDLE_VALUE);

		hOutputFile = CreateFile(outputPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		assert(hInputFile != INVALID_HANDLE_VALUE);

		while (TRUE) {
			key[MAX_WORD_LEN] = _T('\0');
			value[MAX_WORD_LEN] = _T('\0');
			assert(ReadFile(hInputFile, key, MAX_WORD_LEN * sizeof(TCHAR), &nRead1, NULL));
			assert(ReadFile(hInputFile, value, MAX_WORD_LEN * sizeof(TCHAR), &nRead2, NULL));
			if (nRead1 == 0) {
				// end of file
				break;
			}
			if (nRead1 + nRead2 != MAX_WORD_LEN * 2 * sizeof(TCHAR)) {
				_ftprintf(stderr, _T("Read size mismatch 1\n"));
				break;
			}
			DictionaryInsert(&dictionary, key, value);
		}

		DictionaryBinaryWrite(&dictionary, hOutputFile);
		CloseHandle(hOutputFile);
		HandToHandDeliver(&intermediateFileName, outputPath);

		CloseHandle(hInputFile);
	}
	return NULL;
}

DWORD WINAPI MergingThread_f(LPVOID p) {
	TCHAR inputFileName[MAX_PATH];
	LPTSTR outputFileName;
	HANDLE hInputFile, hOutputFile;
	DICTIONARY dictionary;

	outputFileName = (LPTSTR)p;
	DictionaryInit(&dictionary);

	TCHAR key[MAX_WORD_LEN + 1], value[MAX_WORD_LEN + 1];
	DWORD n, i;
	DWORD nRead;

	while (HandToHandAsk(&intermediateFileName, inputFileName)) {
		hInputFile = CreateFile(inputFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		assert(hInputFile != INVALID_HANDLE_VALUE);

		while (TRUE) {
			key[MAX_WORD_LEN] = _T('\0');
			assert(ReadFile(hInputFile, key, MAX_WORD_LEN * sizeof(TCHAR), &nRead, NULL));
			if (nRead == 0) {
				// end of file
				break;
			}
			if (nRead != MAX_WORD_LEN * sizeof(TCHAR)) {
				_ftprintf(stderr, _T("Read size mismatch 2\n"));
				break;
			}
			assert(ReadFile(hInputFile, &n, sizeof(DWORD), &nRead, NULL));
			if (nRead != sizeof(DWORD)) {
				_ftprintf(stderr, _T("Read size mismatch 3\n"));
				break;
			}
			for (i = 0; i < n; i++) {
				assert(ReadFile(hInputFile, value, MAX_WORD_LEN * sizeof(TCHAR), &nRead, NULL));
				if (nRead != MAX_WORD_LEN * sizeof(TCHAR)) {
					_ftprintf(stderr, _T("Read size mismatch 4\n"));
					break;
				}
				DictionaryInsert(&dictionary, key, value);
			}
		}

		CloseHandle(hInputFile);
	}

	hOutputFile = CreateFile(outputFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	assert(hInputFile != INVALID_HANDLE_VALUE);
	DictionaryBinaryWrite(&dictionary, hOutputFile);
	CloseHandle(hOutputFile);
	
	return NULL;
}

BOOL InputNamesInit(LPINPUTNAMES in, LPCTSTR path) {
	InitializeCriticalSection(&in->me);
	in->finished = FALSE;
	in->hFindFile = NULL;
	_tcsncpy(in->path, path, MAX_PATH);
	return TRUE;
}
BOOL InputNamesRead(LPINPUTNAMES in, LPTSTR inputName) {
	WIN32_FIND_DATA wfd;
	TCHAR searchExp[MAX_PATH];

	EnterCriticalSection(&in->me);
	__try {
		if (in->hFindFile == NULL) {
			// first call
			_tcsncpy(searchExp, in->path, MAX_PATH);
			_tcsncat(searchExp, _T("\\*"), MAX_PATH);
			in->hFindFile = FindFirstFile(searchExp, &wfd);
			if (in->hFindFile == INVALID_HANDLE_VALUE) {
				return FALSE;
			}
		} else {
			if (!FindNextFile(in->hFindFile, &wfd)) {
				return FALSE;
			}
		}
		while (FileType(&wfd) != TYPE_FILE) {
			if (!FindNextFile(in->hFindFile, &wfd)) {
				return FALSE;
			}
		}
	}
	__finally {
		LeaveCriticalSection(&in->me);
	}
	_tcsncpy(inputName, wfd.cFileName, MAX_PATH);
	return TRUE;
}

BOOL DictionaryInit(LPDICTIONARY d) {
	d->nTerms = 0;
	d->terms = NULL;
	return TRUE;
}
BOOL DictionaryInsert(LPDICTIONARY d, LPCTSTR key, LPCTSTR value) {
	LPTERM_NODE tn, tn_new;
	LPTRANSLATION tr_old, tr_new;

	tr_new = (LPTRANSLATION)calloc(1, sizeof(TRANSLATION));
	assert(tr_new);
	_tcsncpy(tr_new->value, value, MAX_WORD_LEN);
	
	tn = d->terms;

	if (tn == NULL) {
		// empty dictionary
		tn = (LPTERM_NODE)calloc(1, sizeof(TERM_NODE));
		assert(tn);
		tn->next = NULL;
		// init term
		_tcsncpy(tn->term.key, key, MAX_WORD_LEN);
		tn->term.nTranslations = 0;
		tn->term.translation_list = NULL;
		d->terms = tn;
		d->nTerms++;
	} else {
		while (tn->next != NULL && _tcsncmp(tn->next->term.key, key, MAX_WORD_LEN) < 0) {
			tn = tn->next;
		}
		if (tn->next == NULL) {
			tn_new = (LPTERM_NODE)calloc(1, sizeof(TERM_NODE));
			tn_new->next = tn->next;
			tn->next = tn_new;
			tn = tn_new;
			// init term
			_tcsncpy(tn->term.key, key, MAX_WORD_LEN);
			tn->term.nTranslations = 0;
			tn->term.translation_list = NULL;
			d->terms = tn;
			d->nTerms++;
		} else {
			if (_tcsncmp(tn->next->term.key, key, MAX_WORD_LEN) > 0) {
				tn_new = (LPTERM_NODE)calloc(1, sizeof(TERM_NODE));
				tn_new->next = tn->next;
				tn->next = tn_new;
				tn = tn_new;
				// init term
				_tcsncpy(tn->term.key, key, MAX_WORD_LEN);
				tn->term.nTranslations = 0;
				tn->term.translation_list = NULL;
				d->terms = tn;
				d->nTerms++;
			} else {
				// found key
				tn = tn->next;
			}
		}
	}
	// insert value
	tn->term.nTranslations++;
	tr_old = tn->term.translation_list;
	tn->term.translation_list = tr_new;
	tr_new->next = tr_old;
	return TRUE;
}
BOOL DictionaryBinaryWrite(LPDICTIONARY d, HANDLE hFile) {
	DWORD nWritten, i;
	LPTERM_NODE tn;
	LPTRANSLATION tr;
	tn = d->terms;
	while (tn != NULL) {
		if (!WriteFile(hFile, tn->term.key, MAX_WORD_LEN * sizeof(TCHAR), &nWritten, NULL) || nWritten != MAX_WORD_LEN * sizeof(TCHAR)) {
			return FALSE;
		}
		if (!WriteFile(hFile, &tn->term.nTranslations, sizeof(DWORD), &nWritten, NULL) || nWritten != sizeof(DWORD)) {
			return FALSE;
		}
		i = 0;
		tr = tn->term.translation_list;
		while (tr != NULL) {
			if (!WriteFile(hFile, tr->value, MAX_WORD_LEN * sizeof(TCHAR), &nWritten, NULL) || nWritten != MAX_WORD_LEN * sizeof(TCHAR)) {
				return FALSE;
			}
			tr = tr->next;
			i++;
		}
		assert(i == tn->term.nTranslations);
		tn = tn->next;
	}
	return TRUE;
}

BOOL HandToHandInit(LPHANDTOHAND hth) {
	hth->hRequest = CreateEvent(NULL, FALSE, FALSE, NULL);
	hth->hResponse = CreateEvent(NULL, FALSE, FALSE, NULL);
	hth->closed = FALSE;
	return TRUE;
}
BOOL HandToHandAsk(LPHANDTOHAND hth, LPTSTR fileName) {
	if (hth->closed) {
		return FALSE;
	}
	SetEvent(hth->hRequest);
	if (WaitForSingleObject(hth->hResponse, INFINITE) != WAIT_OBJECT_0) {
		return FALSE;
	}
	if (hth->closed) {
		return FALSE;
	}
	_tcsncpy(fileName, hth->fileName, MAX_PATH);
	return TRUE;
}
BOOL HandToHandDeliver(LPHANDTOHAND hth, LPCTSTR fileName) {
	if (hth->closed) {
		return FALSE;
	}
	if (WaitForSingleObject(hth->hRequest, INFINITE) != WAIT_OBJECT_0) {
		return FALSE;
	}
	_tcsncpy(hth->fileName, fileName, MAX_PATH);
	SetEvent(hth->hResponse);
	return TRUE;
}
BOOL HandToHandClose(LPHANDTOHAND hth) {
	hth->closed = TRUE;
	SetEvent(hth->hResponse);
	return TRUE;
}

static DWORD FileType(LPWIN32_FIND_DATA pFileData) {
	BOOL IsDir;
	DWORD FType;
	FType = TYPE_FILE;
	IsDir = (pFileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	if (IsDir)
		if (lstrcmp(pFileData->cFileName, _T(".")) == 0 || lstrcmp(pFileData->cFileName, _T("..")) == 0)
			FType = TYPE_DOT;
		else FType = TYPE_DIR;
		return FType;
}