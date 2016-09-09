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
	LPTSTR path; // path to search
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
	LPTSTR fileName[MAX_PATH];
} HANDTOHAND, *LPHANDTOHAND;
BOOL HandToHandInit(LPHANDTOHAND);
BOOL HandToHandAsk(LPHANDTOHAND, LPTSTR fileName);
BOOL HandToHandDeliver(LPHANDTOHAND, LPCTSTR fileName);

// global variables
LPTSTR sourceDirectory, destinationDirectory;
INPUTNAMES inputNames;
HANDTOHAND intermediateFileName;

// prototypes
DWORD WINAPI SortingThread_f(LPVOID);
DWORD WINAPI MergingThread_f(LPVOID);

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
	
	assert(WaitForMultipleObjects(n + 1, hThreads, TRUE, INFINITE) == WAIT_OBJECT_0);

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

	while (InputNamesRead(&inputNames, inputName)) {
		_tcsncpy(inputPath, sourceDirectory, MAX_PATH);
		_tcsncat(inputPath, inputName, MAX_PATH);
		_tcsncpy(outputPath, destinationDirectory, MAX_PATH);
		_tcsncat(inputPath, inputName, MAX_PATH);

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
			if (nRead1 + nRead2 != 30 * sizeof(TCHAR)) {
				_ftprintf(stderr, _T("Read size mismatch\n"));
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
				_ftprintf(stderr, _T("Read size mismatch\n"));
				break;
			}
			assert(ReadFile(hInputFile, &n, sizeof(DWORD), &nRead, NULL));
			if (nRead != sizeof(DWORD)) {
				_ftprintf(stderr, _T("Read size mismatch\n"));
				break;
			}
			for (i = 0; i < n; i++) {
				assert(ReadFile(hInputFile, value, MAX_WORD_LEN * sizeof(TCHAR), &nRead, NULL));
				if (nRead != MAX_WORD_LEN * sizeof(TCHAR)) {
					_ftprintf(stderr, _T("Read size mismatch\n"));
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
}