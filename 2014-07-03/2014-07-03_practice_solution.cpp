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

// statistics
typedef struct _STATS {
	CRITICAL_SECTION me;
	INT tot_voters;
	INT tot_wait_time;
} STATS, *LPSTATS;
BOOL StatsInit(LPSTATS);
BOOL StatsDelete(LPSTATS);

typedef struct _TIME {
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

BOOL VoterRead(FILE *, LPVOTER);
BOOL VoterWrite(FILE *, VOTER);


typedef struct _HANDTOHAND {
	//CRITICAL_SECTION me; // to protect structure
	HANDLE hVoterRequest; // the handshake request (manual reset event)
	HANDLE hVoterDelivered; // the handshake has been performed (manual reset event)
	VOTER voter; // data passed hand to hand
	BOOL closed; // no more input
} HANDTOHAND, *LPHANDTOHAND;

BOOL HandToHandInit(LPHANDTOHAND);
BOOL HandToHandRequest(LPHANDTOHAND, LPVOTER);
BOOL HandToHandDeliver(LPHANDTOHAND, VOTER);
BOOL HandToHandClose(LPHANDTOHAND);
BOOL HandToHandDelete(LPHANDTOHAND);


typedef struct _QUEUE {
	CRITICAL_SECTION me;
	INT size;
	INT count;
	INT deq_index;
	INT enq_index;
	LPVOTER voters; // array of voters
	CONDITION_VARIABLE can_dequeue, can_enqueue;
	BOOL closed; // if no writers on the queue (to detect termination)
} QUEUE, *LPQUEUE;

BOOL QueueInit(LPQUEUE, INT);
BOOL QueueDequeue(LPQUEUE, LPVOTER);
BOOL QueueEnqueue(LPQUEUE, VOTER);
BOOL QueueClose(LPQUEUE);
BOOL QueueDelete(LPQUEUE);

// global variables
HANDTOHAND incomingVoters[2];
QUEUE internalQueue;
FILE *outputFile;
TIME timer;
STATS stats;
SYNCHRONIZATION_BARRIER registration_barrier;

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
	FILE *inputFile;
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

	inputFile = _tfopen(inputFileName, _T("r"));
	if (inputFile == NULL) {
		_ftprintf(stderr, _T("Impossible to open input file.\n"));
		return 1;
	}
	outputFile = _tfopen(outputFileName, _T("w"));
	if (outputFile == NULL) {
		_ftprintf(stderr, _T("Impossible to open output file.\n"));
		return 1;
	}
	hVotingStations = (LPHANDLE)calloc(N, sizeof(HANDLE));
	assert(hVotingStations);

	// initialize data
	assert(HandToHandInit(&incomingVoters[MALE]));
	assert(HandToHandInit(&incomingVoters[FEMALE]));
	assert(QueueInit(&internalQueue, M));
	assert(StatsInit(&stats));
	InitializeSynchronizationBarrier(&registration_barrier, 2, 0);

	// start threads
	hWelcomingThread = CreateThread(NULL, 0, Welcoming, inputFile, 0, NULL);
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
	StatsDelete(&stats);
	DeleteSynchronizationBarrier(&registration_barrier);
	return 0;
}

DWORD WINAPI Welcoming(LPVOID p) {
	FILE *file;
	VOTER v;
	int nRead;

	file = (FILE *)p;
	nRead = 0;
	while (VoterRead(file, &v)) {
		if (nRead++ == 0) {
			TimeInit(&timer, v.arrival_time_minutes); // set time reference on first record
		}
		HandToHandDeliver(&incomingVoters[v.sex], v);
	}
	HandToHandClose(&incomingVoters[MALE]);
	HandToHandClose(&incomingVoters[FEMALE]);
	return NULL;
}
DWORD WINAPI RegisteringStation(LPVOID p) {
	VOTER v;
	INT sex = (INT)p;
	while (HandToHandRequest(&incomingVoters[sex], &v)) {
		RegisterVoter(&v);
		QueueEnqueue(&internalQueue, v);
	}
	if (EnterSynchronizationBarrier(&registration_barrier, 0)) {
		// the last registering station
		QueueClose(&internalQueue);
	}
	return NULL;
}
DWORD WINAPI VotingStation(LPVOID p) {
	INT id;
	VOTER v;
	id = (INT)p;
	while (QueueDequeue(&internalQueue, &v)) {
		VoteVoter(&v, id);
		VoterWrite(outputFile, v);
	}
	return NULL;
}

BOOL RegisterVoter(LPVOTER v) {
	INT t;
	TimeGet(&timer, &t);
	_tprintf(_T("%02d:%02d - %s Registering voter %s requiring %d minutes\n"), t / 60, t % 60, (v->sex == MALE)? _T("male") : _T("female"), v->id, v->minutes_to_register);
	TimeWait(&timer, v->minutes_to_register, &t);
	_tprintf(_T("%02d:%02d - %s Registered voter %s\n"), t / 60, t % 60, (v->sex == MALE) ? _T("male") : _T("female"), v->id);
	return TRUE;
}

BOOL VoteVoter(LPVOTER v, INT voting_station_id) {
	INT t;
	TimeGet(&timer, &t);
	_tprintf(_T("%02d:%02d - Voter %s voting at voting station %d requiring %d minutes\n"), t / 60, t % 60, v->id, voting_station_id, v->minutes_to_vote);
	v->voting_station = voting_station_id;
	TimeWait(&timer, v->minutes_to_vote, &t);
	v->completion_time = t;
	_tprintf(_T("%02d:%02d - Voter %s voted at voting station %d\n"), t / 60, t % 60, v->id, voting_station_id);
	return TRUE;
}

BOOL StatsInit(LPSTATS s) {
	InitializeCriticalSection(&s->me);
	s->tot_voters = 0;
	s->tot_wait_time = 0;
	return TRUE;
}

BOOL StatsDelete(LPSTATS s) {
	DeleteCriticalSection(&s->me);
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

// waits val seconds and stores new time in new_val
BOOL TimeWait(LPTIME t, INT val, LPINT new_val) {
	time_t real_time, real_diff;
	
	Sleep(val * 1000);
	real_time = time(NULL);
	real_diff = real_time - t->real_reference; // seconds elapsed from beginning
	*new_val = t->fake_reference + real_diff;
	return TRUE;
}

// read a voter (no concurrency, the welcoming thread does them all)
BOOL VoterRead(FILE *file, LPVOTER v) {
	TCHAR line[MAX_PATH];
	TCHAR sex;
	INT hour, minutes;
	if (_fgetts(line, MAX_PATH, file) == NULL) {
		return FALSE;
	}
	if (_stscanf(line, _T("%s %s %s %c %d:%d %d %d"), v->id, v->name, v->surname, &sex, &hour, &minutes, &v->minutes_to_register, &v->minutes_to_vote) != 8) {
		_ftprintf(stderr, _T("Error in file format\n"));
		return FALSE;
	}
	switch (sex) {
	case _T('M'):
	case _T('m'):
		v->sex = MALE;
		break;
	case _T('F'):
	case _T('f'):
		v->sex = FEMALE;
		break;
	default:
		_ftprintf(stderr, _T("Error in file format\n"));
		return FALSE;
	}
	v->arrival_time_minutes = hour * 60 + minutes;
	return TRUE;
}

// write a voter to output file. Concurrency between voting stations is protected by the stats structure, serializing also file writes
BOOL VoterWrite(FILE *file, VOTER v) {
	EnterCriticalSection(&stats.me);
	__try {
		stats.tot_voters++;
		stats.tot_wait_time += v.completion_time - v.arrival_time_minutes - v.minutes_to_register - v.minutes_to_vote; // time wasted
		if (_ftprintf(file, _T("%s Voting_Station_%d %02d:%02d\n"), v.id, v.voting_station, v.completion_time / 60, v.completion_time % 60) == 0) {
			_ftprintf(stderr, _T("Error writing output file\n"));
			return FALSE;
		}
	}
	__finally {
		LeaveCriticalSection(&stats.me);
	}
	return TRUE;
}

BOOL HandToHandInit(LPHANDTOHAND hth) {
	hth->closed = FALSE;
	hth->hVoterDelivered = CreateEvent(NULL, FALSE, FALSE, NULL); // auto reset
	hth->hVoterRequest = CreateEvent(NULL, FALSE, FALSE, NULL); // auto reset
	if (hth->hVoterDelivered == NULL || hth->hVoterRequest == NULL) {
		return FALSE;
	}
	return TRUE;
}
// request a voter to be stored inside hth->voter
BOOL HandToHandRequest(LPHANDTOHAND hth, LPVOTER v) {
	SetEvent(hth->hVoterRequest); // make request
	// wait for response
	if (WaitForSingleObject(hth->hVoterDelivered, INFINITE) != WAIT_OBJECT_0) {
		_ftprintf(stderr, _T("Error waiting response\n"));
		return FALSE;
	}
	if (hth->closed) {
		return FALSE;
	}
	*v = hth->voter;
	return TRUE;
}
// answer to request and store voter into hth->voter
BOOL HandToHandDeliver(LPHANDTOHAND hth, VOTER v) {
	// wait for request
	if (WaitForSingleObject(hth->hVoterRequest, INFINITE) != WAIT_OBJECT_0) {
		_ftprintf(stderr, _T("Error waiting request\n"));
		return FALSE;
	}
	hth->voter = v;
	SetEvent(hth->hVoterDelivered); // send response
	return TRUE;
}
BOOL HandToHandClose(LPHANDTOHAND hth) {
	// wait for request
	if (WaitForSingleObject(hth->hVoterRequest, INFINITE) != WAIT_OBJECT_0) {
		_ftprintf(stderr, _T("Error waiting request\n"));
		return FALSE;
	}
	hth->closed = TRUE;
	SetEvent(hth->hVoterDelivered); // send response (empty)
	return TRUE;
}
BOOL HandToHandDelete(LPHANDTOHAND hth) {
	CloseHandle(hth->hVoterDelivered);
	CloseHandle(hth->hVoterRequest);
	return TRUE;
}

BOOL QueueInit(LPQUEUE q, INT size) {
	InitializeCriticalSection(&q->me);
	InitializeConditionVariable(&q->can_dequeue);
	InitializeConditionVariable(&q->can_enqueue);
	q->deq_index = 0;
	q->enq_index = 0;
	q->count = 0;
	q->size = size;
	q->closed = FALSE;
	q->voters = (LPVOTER)calloc(size, sizeof(VOTER));
	if (q->voters == NULL) {
		return FALSE;
	}
	return TRUE;
}
BOOL QueueDequeue(LPQUEUE q, LPVOTER v) {
	EnterCriticalSection(&q->me);
	__try {
		while (q->count == 0 && !q->closed) {
			SleepConditionVariableCS(&q->can_dequeue, &q->me, INFINITE);
		}
		if (q->closed) {
			return FALSE;
		}
		q->count--;
		*v = q->voters[q->deq_index];
		q->deq_index = q->deq_index + 1 % q->size;
	}
	__finally {
		WakeAllConditionVariable(&q->can_enqueue);
		LeaveCriticalSection(&q->me);
	}
	return TRUE;
}
BOOL QueueEnqueue(LPQUEUE q, VOTER v) {
	EnterCriticalSection(&q->me);
	__try {
		while (q->count == q->size && !q->closed) {
			SleepConditionVariableCS(&q->can_enqueue, &q->me, INFINITE);
		}
		if (q->closed) {
			return FALSE;
		}
		q->count++;
		q->voters[q->enq_index] = v;
		q->enq_index = q->enq_index + 1 % q->size;
	}
	__finally {
		WakeAllConditionVariable(&q->can_dequeue);
		LeaveCriticalSection(&q->me);
	}
	return TRUE;
}
BOOL QueueClose(LPQUEUE q) {
	EnterCriticalSection(&q->me);
	__try {
		q->closed = TRUE;
	}
	__finally {
		WakeAllConditionVariable(&q->can_dequeue);
		WakeAllConditionVariable(&q->can_enqueue);
		LeaveCriticalSection(&q->me);
	}
	return TRUE;
}
BOOL QueueDelete(LPQUEUE q) {
	DeleteCriticalSection(&q->me);
	free(q->voters);
	return TRUE;
}