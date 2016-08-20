#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <time.h>

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
    int tot_duration;
    int tot_works;
} statistics_t;

// global variable (needed for the controller signal_hndlr)
statistics_t statistics;
int monitor_fd_write;
int *machine_fds_write;
int controller_fd_read;
int k;
int monitor_pid;


void machine_work(int id, int read_fd, int write_fd);
void controller_work(int k);
void monitor_work(int k, int read_fd);
void signal_hndlr(int sig_no);

ssize_t write_interrupted(int fd, const void *buf, size_t count);
ssize_t read_interrupted(int fd, void *buf, size_t count);

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
        controller_fd_read = controller_pipe[0];
        monitor_pid = cpid;
        controller_work(k);
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
    srand(time(NULL));
    //signal(SIGPIPE, SIG_IGN); // ignore SIGPIPE that can be caused by controller dead because already sent stop
    while(1) {
        read(read_fd, &m_in, sizeof(message_t));
        switch(m_in.msg_type) {
            case MSG_STOP:
                m_out.msg_type = MSG_DONE;
                m_out.id = id;
                m_out.time = 0;
                return;
                break;
            case MSG_START:
                sleep_time = rand() % 4 + 1;
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

void controller_work(int k) {
    int i;
    message_t m_in, m_out;
    sigset_t saved_sigset, x;

    sigemptyset(&x);
    sigaddset(&x, SIGUSR1);

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
            read_interrupted(controller_fd_read, &m_in, sizeof(message_t));

            sigprocmask(SIG_BLOCK, &x, &saved_sigset); // protect this region by blocking SIGUSR1
            statistics.tot_duration += m_in.time;
            statistics.tot_works++;
            sigprocmask(SIG_SETMASK, &saved_sigset, NULL); // now SIGUSR1 can be delivered safely
        }
    }
}

void signal_hndlr(int sig_no) {
    int i;
    int tot_duration, tot_works;
    double avg_duration;
    message_t m_out;

    sigset_t saved_sigset, x;

    sigemptyset(&x);
    sigaddset(&x, SIGUSR1);
    sigprocmask(SIG_BLOCK, &x, &saved_sigset);

    switch(sig_no) {
        case SIGALRM:
            // TIMEOUT expired
            m_out.msg_type = MSG_STOP;
            
            for(i = 0; i < k; i++) {
                write(machine_fds_write[i], &m_out, sizeof(message_t));
            }
            kill(monitor_pid, SIGKILL);
            exit(0);
            break;
        case SIGUSR1:
            // request from monitor

            // read and update statistics
            tot_duration = statistics.tot_duration;
            tot_works = statistics.tot_works;
            statistics.tot_duration = 0;
            statistics.tot_works = 0;
            
            avg_duration = tot_duration / tot_works;
            // send statistics to monitor
            write(monitor_fd_write, &avg_duration, sizeof(double));
            break;
    }
    sigprocmask(SIG_SETMASK, &saved_sigset, NULL); // now SIGUSR1 can be delivered again
    return;
}

void monitor_work(int k, int read_fd) {
    char line[255];
    double avg_duration;
    time_t t;
    struct tm tm;
    while(1) {
        fgets(line, 255, stdin);
        kill(getppid(), SIGUSR1);
        read(read_fd, &avg_duration, sizeof(double));
        t = time(NULL);
        tm = *localtime(&t);
        printf("%d/%d/%d average duration: %f", tm.tm_mday, tm.tm_mon, tm.tm_year, avg_duration);
    }
}

ssize_t write_interrupted(int fd, const void *buf, size_t count) {
    ssize_t retval;
    while((retval = write(fd, buf, count)) == -1 && errno == EINTR);
    return retval;
}
ssize_t read_interrupted(int fd, void *buf, size_t count) {
    ssize_t retval;
    while((retval = read(fd, buf, count)) == -1 && errno == EINTR);
    return retval;
}