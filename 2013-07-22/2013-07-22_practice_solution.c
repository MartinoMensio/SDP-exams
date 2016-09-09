#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define CW 0
#define CCW 1

typedef struct _track {
    pthread_mutex_t me;
} track_t;

typedef struct _station {
    track_t tracks[2];
} station_t;

typedef struct _train {
    int station;
    int direction;
    int id;
} train_t;

typedef struct _selector {
    int *choices;
    int remaining;
} selector_t;

station_t *stations;
track_t *tracks;
train_t *trains;
int n_stations;
int n_trains;

selector_t selector;

void select_station_and_track_init(int n);
void select_station_and_track(int *station, int *track);
void *train_f(void *);

int main(int argc, char **argv) {
    int i;
    pthread_t *tids;

    if(argc != 3) {
        fprintf(stderr, "Usage: %s n_stations n_trains\n", argv[0]);
        return 1;
    }
    n_stations = atoi(argv[1]);
    n_trains = atoi(argv[2]);
    if(n_stations <= 0 || n_trains <= 0) {
        fprintf(stderr, "n_stations and n_trains must be positive integers\n");
        return 1;
    }

    tids = calloc(n_trains, sizeof(pthread_t));
    stations = calloc(n_stations, sizeof(station_t));
    trains = calloc(n_trains, sizeof(train_t));
    tracks = calloc(n_stations, sizeof(track_t));
    if(tids == NULL || stations == NULL || trains == NULL) {
        fprintf(stderr, "Error allocating resources\n");
        return 1;
    }
    for(i = 0; i < n_stations; i++) {
        pthread_mutex_init(&stations[i].tracks[CW].me, NULL);
        pthread_mutex_init(&stations[i].tracks[CCW].me, NULL);
        pthread_mutex_init(&tracks[i].me, NULL);
    }
    srand(time(NULL));
    select_station_and_track_init(n_stations);
    for(i = 0; i < n_trains; i++) {
        trains[i].id = i;
        select_station_and_track(&trains[i].station, &trains[i].direction);
        pthread_mutex_lock(&stations[trains[i].station].tracks[trains[i].direction].me);
    }
    for(i = 0; i < n_trains; i++) {
        pthread_create(&tids[i], NULL, train_f, &trains[i]);
        pthread_detach(tids[i]);
    }
    pthread_exit(NULL);
}

void *train_f(void *p) {
    train_t *myself;
    int sleep_time;
    int next_track;
    int next_station;
    myself = (train_t *)p;

    while(1) {
        sleep_time = rand() % 6;
        printf("Train n. %3d, in station %3d going %s\n", myself->id, myself->station, (myself->direction == CW)? "CLOCKWISE" : "COUNTERCLOCKWISE");
        sleep(sleep_time);
        if(myself->direction == CW) {
            next_track = myself->station;
            next_station = (myself->station + 1) % n_stations;
        } else {
            next_track = (myself->station - 1 + n_stations) % n_stations;
            next_station = (myself->station - 1 + n_stations) % n_stations;
        }
        pthread_mutex_lock(&tracks[next_track].me);
        pthread_mutex_unlock(&stations[myself->station].tracks[myself->direction].me);
        printf("Train n. %3d, traveling toward station %3d\n", myself->id, next_station);
        sleep(10);
        pthread_mutex_lock(&stations[next_station].tracks[myself->direction].me);
        pthread_mutex_unlock(&tracks[next_track].me);
        myself->station = next_station;
    }
}

void select_station_and_track_init(int n) {
    int i;
    selector.remaining = n * 2;
    selector.choices = calloc(n * 2, sizeof(int));
    if(selector.choices == NULL) {
        fprintf(stderr, "Error allocating selectors\n");
        exit(1);
    }
    for(i = 0; i < n * 2; i++) {
        selector.choices[i] = i;
    }
    return;
}
void select_station_and_track(int *station, int *track) {
    int choice;
    if(selector.remaining <= 0) {
        fprintf(stderr, "Error selecting station and track\n");
        exit(1);
    }
    choice = rand() % selector.remaining;
    *station = selector.choices[choice] / 2;
    *track = selector.choices[choice] % 2;
    selector.remaining--;
    selector.choices[choice] = selector.choices[selector.remaining];
}