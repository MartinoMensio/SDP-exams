#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

typedef struct _protected_int {
    volatile int val;
    pthread_mutex_t me;
} protected_int_t;
void protected_int_init(protected_int_t *, int);

protected_int_t a_done, b_done, a_cat, b_cat;
volatile int n_merge;
int n;
int *a_array, *b_array;


void *ta_work(void *arg);
void *tb_work(void *arg);

int main(int argc, char ** argv) {
    pthread_t *ta, *tb;
    int i, *ids;
    
    if(argc != 2) {
        fprintf(stderr, "Missing parameter. Usage: %s n\n", argv[0]);
        return 1;
    }
    n = atoi(argv[1]);
    if(n == 0 || n % 2 != 0) {
        fprintf(stderr, "Parameter n must be a positive even number\n");
        return 2;
    }

    ta = calloc(n, sizeof(pthread_t));
    tb = calloc(n, sizeof(pthread_t));
    a_array = calloc(n, sizeof(int));
    b_array = calloc(n, sizeof(int));
    ids = calloc(n, sizeof(int));
    if(!ta || !tb || !a_array || !b_array) {
        fprintf(stderr, "Error in allocation\n");
        return 3;
    }

    srand(time(NULL));

    protected_int_init(&a_done, 0);
    protected_int_init(&b_done, 0);
    protected_int_init(&a_cat, 0);
    protected_int_init(&b_cat, 0);
    n_merge = 0;

    for(i = 0; i < n; i++) {
        ids[i] = i;
        if(pthread_create(&ta[i], NULL, ta_work, &ids[i]) || pthread_create(&tb[i], NULL, tb_work, &ids[i])) {
            fprintf(stderr, "Error creating threads\n");
            return 4;
        }
    }
    pthread_detach(pthread_self());
    pthread_exit(NULL);
}

void *ta_work(void *arg) {
    int t, tmp, id;

    pthread_detach(pthread_self());
    id = *(int *)arg;

    t = rand() % 4;
    sleep(t);
    pthread_mutex_lock(&a_done.me);
    tmp = a_done.val++;
    a_array[tmp] = id;
    pthread_mutex_unlock(&a_done.me);
    tmp++;
    if(tmp % 2 == 0) {
        printf("A%d cats A%d A%d\n", a_array[tmp - 1], a_array[tmp - 2], a_array[tmp - 1]);
        tmp = -1;
        pthread_mutex_lock(&a_cat.me);
        pthread_mutex_lock(&b_cat.me);
        a_cat.val++;
        //printf("a_cat=%d b_cat=%d n_merge=%d\n", a_cat.val, b_cat.val, n_merge);
        if((a_cat.val > n_merge) && (b_cat.val > n_merge)) {
            n_merge++;
            tmp = n_merge * 2; // the selected thread to merge
        }
        pthread_mutex_unlock(&b_cat.me);
        pthread_mutex_unlock(&a_cat.me);
        if(tmp >= 0) {
            printf("\t\t\tA%d merges A%d A%d B%d B%d\n", a_array[tmp - 1], a_array[tmp - 2], a_array[tmp - 1], b_array[tmp - 2], b_array[tmp - 1]);
        }
    }

    pthread_exit(NULL);
}

void *tb_work(void *arg) {
    int t, tmp, id;

    pthread_detach(pthread_self());
    id = *(int *)arg;

    t = rand() % 4;
    sleep(t);
    pthread_mutex_lock(&b_done.me);
    tmp = b_done.val++;
    b_array[tmp] = id;
    pthread_mutex_unlock(&b_done.me);
    tmp++;
    if(tmp % 2 == 0) {
        printf("B%d cats B%d B%d\n", b_array[tmp - 1], b_array[tmp - 2], b_array[tmp - 1]);
        tmp = -1;
        pthread_mutex_lock(&a_cat.me);
        pthread_mutex_lock(&b_cat.me);
        b_cat.val++;
        //printf("a_cat=%d b_cat=%d n_merge=%d\n", a_cat.val, b_cat.val, n_merge);
        if((a_cat.val > n_merge) && (b_cat.val > n_merge)) {
            n_merge++;
            tmp = n_merge * 2; // the selected thread to merge
        }
        pthread_mutex_unlock(&b_cat.me);
        pthread_mutex_unlock(&a_cat.me);
        if(tmp >= 0) {
            printf("\t\t\tB%d merges A%d A%d B%d B%d\n", b_array[tmp - 1], a_array[tmp - 2], a_array[tmp - 1], b_array[tmp - 2], b_array[tmp - 1]);
        }
    }

    pthread_exit(NULL);
}

void protected_int_init(protected_int_t *pi, int val) {
    pthread_mutex_init(&pi->me, NULL);
    pi->val = val;
}