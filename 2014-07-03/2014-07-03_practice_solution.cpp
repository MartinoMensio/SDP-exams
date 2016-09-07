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

#define ID_LEN 16
#define NAME_LEN 20
#define SURNAME_LEN NAME_LEN
#define MALE 0
#define FEMALE 1

typedef struct _TIME {
	// TODO
	//CRITICAL_SECTION me;
	INT fake_reference; // the value passed by init
	time_t real_reference; // the value of time when init called
} TIME, *LPTIME;
BOOL TimeInit(LPTIME, INT val);
BOOL TimeGet(LPTIME, LPINT val);
BOOL TimeWait(LPTIME, INT val, LPINT newVal);


typedef struct _VOTER {
	TCHAR id[ID_LEN + 1];
	TCHAR name[NAME_LEN + 1];
	TCHAR surname[SURNAME_LEN + 1];
	INT sex;
	INT arrival_time_minutes;
	INT minutes_to_register;
	INT minutes_to_vote;

	INT completion_time; // time (absolute) after having voted
	INT voting_station;
} VOTER, *LPVOTER;

BOOL VoterRead(HANDLE, LPVOTER);
BOOL VoterWrite(HANDLE, VOTER);


typedef struct _HANDTOHAND {
	//CRITICAL_SECTION me; // to protect structure
	HANDLE hVoterRequest; // the handshake request (manual reset event)
	HANDLE hVoterDelivered; // the handshake has been performed (manual reset event)
	VOTER voter; // data passed hand to hand
} HANDTOHAND, *LPHANDTOHAND;

BOOL HandToHandInit(LPHANDTOHAND);
BOOL HandToHandRequest(LPHANDTOHAND, LPVOTER);
BOOL HandToHandDeliver(LPHANDTOHAND, VOTER);
BOOL HandToHandDelete(LPHANDTOHAND);


typedef struct _QUEUE {
	CRITICAL_SECTION me;
	INT size;
	INT deq_index;
	INT enq_index;
	LPVOTER voters; // array of voters
	CONDITION_VARIABLE can_dequeue, can_enqueue;
} QUEUE, *LPQUEUE;

BOOL QueueInit(LPQUEUE, INT);
BOOL QueueDequeue(LPQUEUE, LPVOTER);
BOOL QueueEnqueue(LPQUEUE, VOTER);
BOOL QueueDelete(LPQUEUE);

// global variables
HANDTOHAND incomingVoters[2];
QUEUE internalQueue;
HANDLE hOutputFile;
TIME timer;

// prototypes
DWORD WINAPI Welcoming(LPVOID);
DWORD WINAPI RegisteringStation(LPVOID);
DWORD WINAPI VotingStation(LPVOID);
BOOL RegisterVoter(LPVOTER);
BOOL VoteVoter(LPVOTER, INT votingStationId);

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
	assert(HandToHandInit(&incomingVoters[MALE]));
	assert(HandToHandInit(&incomingVoters[FEMALE]));
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


	HandToHandDelete(&incomingVoters[MALE]);
	HandToHandDelete(&incomingVoters[FEMALE]);
	QueueDelete(&internalQueue);
	return 0;
}

DWORD WINAPI Welcoming(LPVOID p) {
	HANDLE hFile;
	VOTER v;

	hFile = (HANDLE)p;
	while (VoterRead(hFile, &v)) {
		HandToHandDeliver(&incomingVoters[v.sex], v);
	}
	return NULL;
}
DWORD WINAPI RegisteringStation(LPVOID p) {
	VOTER v;
	INT sex = (INT)p;
	while (HandToHandRequest(&incomingVoters[sex], &v)) {
		RegisterVoter(&v);
		QueueEnqueue(&internalQueue, v);
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

BOOL RegisterVoter(LPVOTER v) {
	INT t;
	TimeGet(&timer, &t);
	_tprintf(_T("%02d:%02d - %s Registering voter %s requiring %d minutes\n"), t / 60, t % 60, (v->sex == MALE)? "male" : "female", v->id, v->minutes_to_register);
	TimeWait(&timer, v->minutes_to_register, &t);
	_tprintf(_T("%02d:%02d - %s Registered voter %s\n"), t / 60, t % 60, (v->sex == MALE) ? "male" : "female", v->id);
	return TRUE;
}

BOOL VoteVoter(LPVOTER v, INT voting_station_id) {
	INT t;
	TimeGet(&timer, &t);
	_tprintf(_T("%02d:%02d - Voter %s voting at voting station %d requiring %d minutes\n"), t / 60, t % 60, v->id, voting_station_id, v->minutes_to_vote);
	TimeWait(&timer, v->minutes_to_vote, &t);
	_tprintf(_T("%02d:%02d - Voter %s voted at voting station %d\n"), t / 60, t % 60, v->id, voting_station_id);
	return TRUE;
}

// initiates the structure with useful information to create fake times
BOOL TimeInit(LPTIME t, INT val) {
	t->real_reference = time(NULL);
	t->fake_reference = val;
	return TRUE;
}

// stores new fake time into val
BOOL TimeGet(LPTIME t, LPINT val) {
	time_t real_time, real_diff;
	real_time = time(NULL);
	real_diff = real_time - t->real_reference; // seconds elapsed from beginning
	*val = t->fake_reference + real_diff;
	
	return TRUE;
}

BOOL TimeWait(LPTIME t, INT val, LPINT new_val) {
	time_t real_time, real_diff;
	
	Sleep(val * 1000);
	real_time = time(NULL);
	real_diff = real_time - t->real_reference; // seconds elapsed from beginning
	*new_val = t->fake_reference + real_diff;
	return TRUE;
}