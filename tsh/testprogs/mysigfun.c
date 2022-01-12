/*
 * mysigfun.c - Weird signal handlers
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>

#include "config.h"
#include "testprogs/helper.h"

static int syncfd, standalone;

void sigalrm_handler(int signum) {
    _exit(0);
}

void sigint_handler(int signum) {
    kill(getpid(), SIGSTOP);
}

void sigtstp_handler(int signum) {
    raise(SIGQUIT);
}

int main(int argc, const char *argv[]) {
    Signal(SIGALRM, sigalrm_handler);
    Signal(SIGINT,  sigint_handler );
    Signal(SIGTSTP, sigtstp_handler);

    alarm(JOB_TIMEOUT);

    /*
     * If running under the driver, get the number of the driver's
     * synchronizing domain socket descriptor.
     */
    standalone = get_syncfd(&syncfd);

    /*
     * If the job is being run by the driver, then synchronize with
     * the driver over its synchronizing domain socket.  Ignore any
     * command line argument.
     */

    if (!standalone) {
        alarm(JOB_TIMEOUT);

        sync_signal(syncfd);
        sync_wait(syncfd);

        exit(0);

    // Otherwise spin until timing out
    } else {
        if (argc > 1) {
            alarm(atoi(argv[1]));
        } else {
            alarm(JOB_TIMEOUT);
        }
        while(1);
    }

    /* Control should never reach here */
    exit(1);
}
