/*
Both threads will iterate the same number of times:
numerator: from i = n to i >= n - k + 1     --> k times
denominator: from i = 1 to i <= k           --> k times
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct _barrier {
    volatile int n;
    pthread_mutex_t me;
    sem_t s;
} barrier_t;

barrier_t b, b2;

int n, k;

int numer, denom;
volatile double result = 1;

void* numerator(void *);
void* denominator(void *);

int main(int argc, char **argv) {
    pthread_t numer_tid, denom_tid;

    // check parameters
    if(argc != 3) {
        fprintf(stderr, "Usage: %s n k\n", argv[0]);
        return 1;
    }
    if(!sscanf(argv[1], "%d", &n) || !sscanf(argv[2], "%d", &k) || n < 0 || k < 0) {
        fprintf(stderr, "Parameters n and k must be positive integers\n");
        return 1;
    }
    // initialize barriers
    b.n = 0;
    pthread_mutex_init(&b.me, NULL);
    sem_init(&b.s, 0, 0);
    b2.n = 0;
    pthread_mutex_init(&b2.me, NULL);
    sem_init(&b2.s, 0, 0);

    // create threads
    if(pthread_create(&numer_tid, NULL, numerator, NULL) == -1) {
        perror("Impossible to create numerator thread");
        return 1;
    }
    if(pthread_create(&denom_tid, NULL, denominator, NULL) == -1) {
        perror("Impossible to create denominator thread");
        return 1;
    }
    // wait threads termination
    pthread_join(numer_tid, NULL);
    pthread_join(denom_tid, NULL);
    // print result
    printf("Result = %d\n", (int)result);
    return 0;
}

void* numerator(void *p) {
    int i;
    int is_last;
    for(i = n; i >= n - k + 1; i -= 2) {
        if(i > n - k + 1) {
            // two numbers available
            numer = i * (i - 1);
        } else {
            // last number is alone
            numer = i;
        }
        // first barrier
        pthread_mutex_lock(&b.me);
        is_last = b.n;
        b.n = (b.n + 1) % 2; // 0 1 0 1 ...
        if(is_last) {
            result *= ((double)numer) / denom;
            // release barrier 1
            sem_post(&b.s);
            sem_post(&b.s);
        }
        pthread_mutex_unlock(&b.me);
        sem_wait(&b.s);

        // second barrier (needed because threads are cyclic)
        pthread_mutex_lock(&b2.me);
        is_last = b2.n;
        b2.n = (b2.n + 1) % 2;
        if(is_last) {
            sem_post(&b2.s);
            sem_post(&b2.s);
        }
        pthread_mutex_unlock(&b2.me);
        sem_wait(&b2.s);
    }
    return NULL;
}
void* denominator(void *p) {
    int i;
    int is_last;
    for(i = 1; i <= k; i += 2) {
        if(i < k) {
            // two numbers available
            denom = i * (i + 1);
        } else {
            // last number is alone
            denom = i;
        }
        // first barrier
        pthread_mutex_lock(&b.me);
        is_last = b.n;
        b.n = (b.n + 1) % 2; // 0 1 0 1 ...
        if(is_last) {
            result *= ((double)numer) / denom;
            // release barrier 1
            sem_post(&b.s);
            sem_post(&b.s);
        }
        pthread_mutex_unlock(&b.me);
        sem_wait(&b.s);

        // second barrier (needed because threads are cyclic)
        pthread_mutex_lock(&b2.me);
        is_last = b2.n;
        b2.n = (b2.n + 1) % 2;
        if(is_last) {
            sem_post(&b2.s);
            sem_post(&b2.s);
        }
        pthread_mutex_unlock(&b2.me);
        sem_wait(&b2.s);
    }
    return NULL;
}