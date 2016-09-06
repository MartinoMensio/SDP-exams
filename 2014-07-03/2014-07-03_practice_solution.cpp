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

#define ID_LEN 16
#define NAME_LEN 20
#define SURNAME_LEN NAME_LEN
#define MALE 0
#define FEMALE 1

typedef struct _VOTER {
	TCHAR id[ID_LEN + 1];
	TCHAR name[NAME_LEN + 1];
	TCHAR surname[SURNAME_LEN + 1];
	INT sex;
	INT arrival_time_hours;
	INT arrival_time_minutes;
	INT minutes_to_register;
	INT minutes_to_vote;

	INT completion_time;
	INT voting_station;
} VOTER, *LPVOTER;

BOOL VoterRead(HANDLE, LPVOTER);
BOOL VoterWrite(HANDLE, VOTER);


typedef struct _SWITCH {
	CRITICAL_SECTION me; // to protect structure
	LPVOTER male, female; // slots
	CONDITION_VARIABLE male_ready, female_ready; // signalled when the relative slot is occupied
	CONDITION_VARIABLE free_ready; // signalled when a slot is free
} SWITCH, *LPSWITCH;

BOOL SwitchInit(LPSWITCH);
BOOL SwitchReadM(LPSWITCH, LPVOTER);
BOOL SwitchReadF(LPSWITCH, LPVOTER);
BOOL SwitchFillM(LPSWITCH, VOTER);
BOOL SwitchFillF(LPSWITCH, VOTER);
BOOL SwitchDelete(LPSWITCH);


typedef struct _QUEUE {
	CRITICAL_SECTION me;
	INT size;
	INT deq_index;
	INT enq_index;
	LPVOTER voters; // array of pointers
	CONDITION_VARIABLE can_dequeue, can_enqueue;
} QUEUE, *LPQUEUE;

BOOL QueueInit(LPQUEUE, INT);
BOOL QueueDequeue(LPQUEUE, LPVOTER);
BOOL QueueEnqueue(LPQUEUE, VOTER);
BOOL QueueDelete(LPQUEUE);

// global variables
SWITCH incomingVoters;
QUEUE internalQueue;
HANDLE hOutputFile;

// prototypes
DWORD WINAPI Welcoming(LPVOID);
DWORD WINAPI RegisteringStation(LPVOID);
DWORD WINAPI VotingStation(LPVOID);

INT _tmain(INT argc, LPTSTR argv[]) {
	INT N, M;
	LPTSTR inputFileName, outputFileName;
	HANDLE hWelcomingThread;
	HANDLE hRegisteringStations[2];
	LPHANDLE hVotingStations;
	HANDLE hInputFile;
	INT i;
	
	if (argc != 5) {
		_ftprintf(stderr, _T("Usage: %s N M\n"), argv[0]);
		return 1;
	}

	inputFileName = argv[1];
	N = _ttoi(argv[2]);
	M = _ttoi(argv[3]);
	outputFileName = argv[4];
	if (N <= 0 || M <= 0) {
		_ftprintf(stderr, _T("N and M must be positive integers\n"));
		return 1;
	}

	hInputFile = CreateFile(inputFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hInputFile == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Impossible to open input file. Error: %x\n"), GetLastError());
		return 1;
	}
	hOutputFile = CreateFile(outputFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOutputFile == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Impossible to open output file. Error: %x\n"), GetLastError());
		return 1;
	}
	hVotingStations = (LPHANDLE)calloc(N, sizeof(HANDLE));
	assert(hVotingStations);

	// initialize data
	assert(SwitchInit(&incomingVoters));
	assert(QueueInit(&internalQueue, M));

	// start threads
	hWelcomingThread = CreateThread(NULL, 0, Welcoming, hInputFile, 0, NULL);
	hRegisteringStations[MALE] = CreateThread(NULL, 0, RegisteringStation, (LPVOID)MALE, 0, NULL);
	hRegisteringStations[FEMALE] = CreateThread(NULL, 0, RegisteringStation, (LPVOID)FEMALE, 0, NULL);
	assert(hWelcomingThread && hRegisteringStations[MALE] && hRegisteringStations[FEMALE]);
	for (i = 0; i < N; i++) {
		hVotingStations[i] = CreateThread(NULL, 0, VotingStation, (LPVOID)i, 0, NULL);
		assert(hVotingStations[i]);
	}
	WaitForSingleObject(hWelcomingThread, INFINITE);
	WaitForMultipleObjects(2, hRegisteringStations, TRUE, INFINITE);
	WaitForMultipleObjects(N, hVotingStations, TRUE, INFINITE);

	// TODO process results

	return 0;
}

DWORD WINAPI Welcoming(LPVOID p) {
	HANDLE hFile;
	VOTER v;

	hFile = (HANDLE)p;
	while (VoterRead(hFile, &v)) {
		if (v.sex == MALE) {
			SwitchFillM(&incomingVoters, v);
		} else if (v.sex == FEMALE) {
			SwitchFillF(&incomingVoters, v);
		}
	}
	return NULL;
}
DWORD WINAPI RegisteringStation(LPVOID p) {
	VOTER v;
	INT sex = (INT)p;
	switch (sex) {
	case MALE:
		while (SwitchReadM(&incomingVoters, &v)) {
			RegisterVoter(&v);
			QueueEnqueue(&internalQueue, v);
		}
		break;
	case FEMALE:
		while (SwitchReadM(&incomingVoters, &v)) {
			RegisterVoter(&v);
			QueueEnqueue(&internalQueue, v);
		}
		break;
	default:
		break;
	}
	return NULL;
}
DWORD WINAPI VotingStation(LPVOID p) {
	INT id;
	VOTER v;
	id = (INT)p;
	while (QueueDequeue(&internalQueue, &v)) {
		VoteVoter(&v, id);
		VoterWrite(hOutputFile, v);
	}
	return NULL;
}