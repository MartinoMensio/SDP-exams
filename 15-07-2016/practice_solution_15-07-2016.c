#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthred.h>

#define MSG_STOP 0
#define MSG_START 1
#define MSG_DONE 2

#define TIMEOUT 60

typedef struct _message {
    int msg_type;
    int id;
    int time;
} message_t; // this struct is little therefore writes are atomic

typedef struct _statistics {
    pthread_mutex_t me; // protect the structure
    int tot_duration;
    int tot_works;
} statistics_t;

// global variable for the controller signal_hndlr
statistics_t statistics;
int monitor_fd_write;
int machine_fds_write;
int k;


void machine_work(int id, int read_fd, int write_fd);
void controller_work(int k, int read_fd, int monitor_pid);
void monitor_work(int k, int read_fd);
void signal_hndlr(int sig_no);

int main(int argc, char **argv) {
    int i;
    int **machine_pipes;
    int controller_pipe[2];
    int monitor_pipe[2];
    int cpid;

    // parameter checking
    if(argc != 2) {
        fprintf(stderr, "Error. Usage: %s K\n", argv[0]);
        return 1;
    }
    k = atoi(argv[1]);
    if(k == 0) {
        fprintf(stderr, "Error. Parameter K must be a positive integer\n");
        return 1;
    }

    machine_pipes = calloc(k, sizeof(int *));
    if(machine_pipes == NULL) {
        fprintf(stderr, "Error allocating pipes\n");
        return 1;
    }
    machine_fds_write = calloc(k, sizeof(int));
    if(machine_fds_write == NULL) {
        fprintf(stderr, "Error allocating fds\n");
        return 1;
    }

    if(pipe(controller_pipe) == -1) {
        fprintf(stderr, "Error creating pipe\n");
        return 1;
    }

    for(i = 0; i < k; i++) {
        machine_pipes[i] = calloc(2, sizeof(int));
        if(pipe(machine_pipes[i]) == -1) {
            perror("Error creating pipe");
            return 1;
        }
        cpid = fork();
        if(cpid == -1) {
            perror("Error forking");
            return 1;
        }
        if(cpid) {
            // father
            close(machine_pipes[i][0]); // close read end of child pipe
            machine_fds_write[i] = machine_pipes[i][1]; // copy write fd of this machine
            continue;
        } else {
            // child (machine tool)
            close(machine_pipes[i][1]); // close write end of child pipe
            close(controller_pipe[0]); // close read end of controller pipe
            machine_work(i, machine_pipes[i][0], controller_pipe[1]);
            return 0;
        }
    }

    close(controller_pipe[1]); // close write end of controller pipe

    if(pipe(monitor_pipe) == -1) {
        perror("Error creating monitor pipe");
        return 1;
    }

    if(signal(SIGUSR1, signal_hndlr) == SIG_ERR) {
        perror("Error calling signal");
        return 1;
    }

    cpid = fork();
    if(cpid == -1) {
        perror("Error forking");
        return 1;
    }
    if(cpid) {
        // father (controller)
        close(monitor_pipe[0]); // close read end of monitor pipe
        controller_work(k, controller_pipe[0], cpid);
    } else {
        // child (monitor)
        close(controller_pipe[0]);
        for(i = 0; i < k; i++) {
            close(machine_pipes[i][1]); // close write end of machine pipes
        }
        close(monitor_pipe[1]); // close write end of monitor pipe
        monitor_work(k, monitor_pipe[0]);
    }
    return 0;
}

void machine_work(int id, int read_fd, int write_fd) {
    message_t m_in, m_out;
    int sleep_time;
    int stop = 0;
    srand(time(NULL));
    while(!stop) {
        read(read_fd, &m_in, sizeof(message_t));
        switch(m_in.msg_type) {
            case MSG_STOP:
                m_out.msg_type = MSG_DONE;
                m_out.id = id;
                m_out.time = 0;
                stop = 1;
                break;
            case MSG_START:
                sleep_time = rand() % 4 + 1
                sleep(sleep_time);
                m_out.msg_type = MSG_DONE;
                m_out.id = id;
                m_out.time = sleep_time;
                break;
            default:
                fprintf(stderr, "Unknown message type received: %d\n", m_in.msg_type);
                return;
        }
        write(write_fd, &m_out, sizeof(message_t));
    }
}

void controller_work(int k, int read_fd, int monitor_pid) {
    int i;
    message_t m_in, m_out;

    pthread_mutex_init(&statistics.me, NULL);
    statistics.tot_duration = 0;
    statistics.tot_works = 0;

    if(signal(SIGALRM, signal_hndlr) == SIG_ERR) {
        perror("Error calling signal");
        return;
    }
    alarm(TIMEOUT);
    while(1) {
        m_out.msg_type = MSG_START;
        for(i = 0; i < k; i++) {
            write_interrupted(machine_fds_write[i], &m_out, sizeof(message_t));
        }
        for(i = 0; i < k; i++) {
            read_interrupted(read_fd, &m_in, sizeof(message_t));

            pthread_mutex_lock(&statistics.me); // signal_hndlr could read dirty values there
            statistics.tot_duration += m_in.time;
            statistics.tot_works++;
            pthread_mutex_unlock(&statistics.me);
        }
    }
}

void signal_hndlr(int sig_no) {
    int i;
    int tot_duration, tot_works;
    double avg_duration;
    message_t m_out;

    switch(sig_no) {
        case SIGALARM:
        // TIMEOUT expired
        m_out.msg_type = MSG_STOP;
        for(i = 0; i < k; i++) {
            write(machine_fds_write[i], &m_out, sizeof(message_t));
        }
        break;
        case SIGUSR1:
        // request from monitor
        if(pthread_mutex_trylock(&statistics.me) == EBUSY) {
            // inside
            printf("controller is inside the critical region. Let's try to wait a bit\n");
            kill(getpid(), sig_no);
            return;
        }
        // read and update statistics
        tot_duration = statistics.tot_duration;
        tot_works = statistics.tot_works;
        statistics.tot_duration = 0;
        statistics.tot_works = 0;
        pthread_mutex_unlock(&statistics.me);

        avg_duration = tot_duration / tot_works;
        // send statistics to monitor
        write(monitor_fd_write, &avg_duration, sizeof(double));
        break;
    }
}

void monitor_work(int k, int read_fd);