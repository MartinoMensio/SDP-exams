#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

typedef struct _counter {
    int value;
    pthread_mutex_t me;
    pthread_cond_t cond;
} counter_t;

counter_t na_count = {0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
counter_t cl_count = {0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

int k;
int *na_done;
int *cl_done;

void *na_gen(void*);
void *cl_gen(void*);
void *na_f(void*);
void *cl_f(void*);

int main(int argc, char **argv) {
    pthread_t tid_na, tid_cl;

    if(argc != 2) {
        fprintf(stderr, "Usage: %s k\n", argv[0]);
        return 1;
    }

    k = atoi(argv[1]);
    if(k <= 0) {
        fprintf(stderr, "Parameter k must be a positive integer\n");
        return 1;
    }

    srand(time(NULL));

    na_done = calloc(k, sizeof(int));
    cl_done = calloc(k, sizeof(int));
    if(na_done == NULL || cl_done == NULL) {
        fprintf(stderr, "Error allocating memory\n");
        return 1;
    }

    if(pthread_create(&tid_na, NULL, na_gen, NULL) || pthread_create(&tid_cl, NULL, cl_gen, NULL)) {
        perror("Error generating atom generators");
        return 1;
    }

    pthread_detach(tid_na);
    pthread_detach(tid_cl);

    pthread_exit(NULL);
}

void *na_gen(void *p) {
    pthread_t tid;
    int i;

    for(i = 0; i < k; i++) {
        sleep(rand() % 5);
        if(pthread_create(&tid, NULL, na_f, (void *)i)) {
            perror("Error creating na thread");
            exit(1);
        }
        pthread_detach(tid);
    }

    pthread_exit(NULL);
}

void *cl_gen(void *p) {
    pthread_t tid;
    int i;

    for(i = k; i < 2 * k; i++) {
        sleep(rand() % 5);
        if(pthread_create(&tid, NULL, cl_f, (void *)i)) {
            perror("Error creating cl thread\n");
            exit(1);
        }
        pthread_detach(tid);
    }

    pthread_exit(NULL);
}

void *na_f(void *p) {
    int id, tmp;
    id = (int)p;

    pthread_mutex_lock(&na_count.me);
    tmp = na_count.value++;
    na_done[tmp] = id;
    pthread_cond_broadcast(&na_count.cond);
    pthread_mutex_unlock(&na_count.me);

    pthread_mutex_lock(&cl_count.me);
    while(cl_count.value <= tmp) {
        pthread_cond_wait(&cl_count.cond, &cl_count.me);
    }
    printf("id: %d - Na %d Cl %d\n", id, na_done[tmp], cl_done[tmp]);
    pthread_mutex_unlock(&cl_count.me);

    pthread_exit(NULL);
}

void *cl_f(void *p) {
    int id, tmp;
    id = (int)p;

    pthread_mutex_lock(&cl_count.me);
    tmp = cl_count.value++;
    cl_done[tmp] = id;
    pthread_cond_broadcast(&cl_count.cond);
    pthread_mutex_unlock(&cl_count.me);

    pthread_mutex_lock(&na_count.me);
    while(na_count.value <= tmp) {
        pthread_cond_wait(&na_count.cond, &na_count.me);
    }
    printf("id: %d - Na %d Cl %d\n", id, na_done[tmp], cl_done[tmp]);
    pthread_mutex_unlock(&na_count.me);

    pthread_exit(NULL);
}