/**
 * @file tsh.c
 * @brief A tiny shell program with job control
 *
 * Shell has built in commands:
 *
 * fg job: resumes job "job" and runs it in the foreground
 * bg: resumes job "job" and runs it in the background
 * jobs: prints the jobs currently running
 * quit: exits shell
 *
 *
 *
 * The shell can run programs if a path is specified
 * The shell also supports input and output redirection
 * with < and >, respectively, along with some target file
 *
 *
 * @author Jack Stellwagen <jstellwa@andrew.cmu.edu>
 */

#include "csapp.h"
#include "tsh_helper.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * If DEBUG is defined, enable contracts and printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

volatile sig_atomic_t pid;
/* Function prototypes */
void eval(const char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);
void cleanup(void);

/**
 * @brief <Write main's function header documentation. What does main do?>
 *
 * TODO: Delete this comment and replace it with your own.
 *
 * "Each function should be prefaced with a comment describing the purpose
 *  of the function (in a sentence or two), the function's arguments and
 *  return value, any error cases that are relevant to the caller,
 *  any pertinent side effects, and any assumptions that the function makes."
 */
int main(int argc, char **argv) {
    int c;
    char cmdline[MAXLINE_TSH]; // Cmdline for fgets
    bool emit_prompt = true;   // Emit prompt (default)

    // Redirect stderr to stdout (so that driver will get all output
    // on the pipe connected to stdout)
    if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
        perror("dup2 error");
        exit(1);
    }

    // Parse the command line
    while ((c = getopt(argc, argv, "hvp")) != -1) {
        switch (c) {
        case 'h': // Prints help message
            usage();
            break;
        case 'v': // Emits additional diagnostic info
            verbose = true;
            break;
        case 'p': // Disables prompt printing
            emit_prompt = false;
            break;
        default:
            usage();
        }
    }

    // Create environment variable
    if (putenv("MY_ENV=42") < 0) {
        perror("putenv error");
        exit(1);
    }

    // Set buffering mode of stdout to line buffering.
    // This prevents lines from being printed in the wrong order.
    if (setvbuf(stdout, NULL, _IOLBF, 0) < 0) {
        perror("setvbuf error");
        exit(1);
    }

    // Initialize the job list
    init_job_list();

    // Register a function to clean up the job list on program termination.
    // The function may not run in the case of abnormal termination (e.g. when
    // using exit or terminating due to a signal handler), so in those cases,
    // we trust that the OS will clean up any remaining resources.
    if (atexit(cleanup) < 0) {
        perror("atexit error");
        exit(1);
    }

    // Install the signal handlers
    Signal(SIGINT, sigint_handler);   // Handles Ctrl-C
    Signal(SIGTSTP, sigtstp_handler); // Handles Ctrl-Z
    Signal(SIGCHLD, sigchld_handler); // Handles terminated or stopped child

    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    Signal(SIGQUIT, sigquit_handler);

    // Execute the shell's read/eval loop
    while (true) {
        if (emit_prompt) {
            printf("%s", prompt);

            // We must flush stdout since we are not printing a full line.
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin)) {
            perror("fgets error");
            exit(1);
        }

        if (feof(stdin)) {
            // End of file (Ctrl-D)
            printf("\n");
            return 0;
        }

        // Remove any trailing newline
        char *newline = strchr(cmdline, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }

        // Evaluate the command line
        eval(cmdline);
    }

    return -1; // control never reaches here
}

/**
 * @brief Check if commandline id makes sense
 *
 **/
bool valid_id(char *s) {
    return (s[0] == '%' && isdigit(s[1])) || isdigit(s[0]);
}

/**
 * @brief gets the jid from a command line argument
 * in the form %jid or pid
 *
 * returns the jid or 0 if it does not exist
 * requires s is a valid ID
 **/
jid_t get_jid(char *s) {
    jid_t jid;
    pid_t id;
    if (s[0] == '%') {
        jid = atoi(&s[1]);
    } else {
        id = atoi(s);
        jid = job_from_pid(id);
    }
    if (job_exists(jid))
        return jid;

    sio_printf("%s: No such job\n", s);
    return 0;
}

/**
 * @brief Deal with and execute builtin commandline arguments
 * Specifically deals with jobs, fg, bg, and quit
 *
 * @return 1 if commandline argument is builtin, 0 otherwise
 *
 */
int builtin_command(struct cmdline_tokens token, sigset_t prev) {
    builtin_state builtin = token.builtin;
    jid_t jid;
    pid_t id;

    int out;

    if (builtin == BUILTIN_NONE) {
        return 0;

    } else if (builtin == BUILTIN_QUIT) {
        exit(0);

    } else if (builtin == BUILTIN_JOBS) {
        if (token.outfile != NULL) {
            out = open(token.outfile, O_TRUNC | O_CREAT | O_WRONLY,
                       S_IWUSR | S_IRUSR | S_IROTH | S_IRGRP);

            if (out == -1) {
                perror(token.outfile);
                exit(0);
            }
            list_jobs(out);
            close(out);
        } else
            list_jobs(STDOUT_FILENO);

    } else if (builtin == BUILTIN_BG) {
        if (token.argv[1] == NULL) {
            sio_printf("bg command requires PID or %%jobid argument\n");
            return 1;
        }

        if (valid_id(token.argv[1])) {
            jid = get_jid(token.argv[1]);

            if (jid == 0)
                return 1;

            id = job_get_pid(jid);

            kill(-id, SIGCONT);
            job_set_state(jid, BG);
            sio_printf("[%d] (%d) %s\n", jid, id, job_get_cmdline(jid));

        } else
            sio_printf("bg: argument must be a PID or %%jobid\n");

    } else if (builtin == BUILTIN_FG) {
        if (token.argv[1] == NULL) {
            sio_printf("fg command requires PID or %%jobid argument\n");
            return 1;
        }
        if (valid_id(token.argv[1])) {
            jid = get_jid(token.argv[1]);

            if (jid == 0)
                return 1;

            id = job_get_pid(jid);

            kill(-id, SIGCONT);
            job_set_state(jid, FG);
            pid = 0;
            // wait for the newly created foreground process
            while (!pid && (fg_job() != 0)) {
                sigsuspend(&prev);
            }

        } else
            sio_printf("fg: argument must be a PID or %%jobid\n");
    }

    return 1;
}

/**
 * @brief Takes the commandline and executes the arguemnts or raises the
 * proper errors
 *
 * First checks if the command is builtin and deals with it accordingly
 * otherwise forks a child process to execture the commandline argument ,
 * dealing with the necessary errors along the way
 *
 * Modeifies the job list according the ongoing/stopped processes
 *
 */
void eval(const char *cmdline) {
    parseline_return parse_result;
    struct cmdline_tokens token;
    pid_t child_pid;
    jid_t child_jid;

    int new_stdin, new_stdout;

    sigset_t mask_all, mask_3, prev;

    sigfillset(&mask_all);
    sigemptyset(&mask_3);
    sigaddset(&mask_3, SIGCHLD);
    sigaddset(&mask_3, SIGINT);
    sigaddset(&mask_3, SIGTSTP);

    // Parse command line
    parse_result = parseline(cmdline, &token);

    if (parse_result == PARSELINE_ERROR || parse_result == PARSELINE_EMPTY) {
        return;
    }

    // block the necessary masks
    sigprocmask(SIG_BLOCK, &mask_3, &prev);

    if (!builtin_command(token, prev)) {
        if ((pid = fork()) == 0) {

            // IO redirection checks
            if (token.infile != NULL) {

                new_stdin = open(token.infile, O_RDONLY,
                                 S_IWUSR | S_IRUSR | S_IROTH | S_IRGRP);

                // catch errors from open
                if (new_stdin == -1) {
                    perror(token.infile);
                    exit(0);
                }
                dup2(new_stdin, STDIN_FILENO);
                close(new_stdin);
            }
            if (token.outfile != NULL) {
                new_stdout = open(token.outfile, O_CREAT | O_TRUNC | O_WRONLY,
                                  S_IWUSR | S_IRUSR | S_IROTH | S_IRGRP);

                // catch errors from open
                if (new_stdout == -1) {

                    perror(token.outfile);
                    exit(0);
                }

                dup2(new_stdout, STDOUT_FILENO);

                close(new_stdout);
            }

            sigprocmask(SIG_SETMASK, &prev, NULL);
            setpgid(0, 0);

            // execute command line via child
            execve(token.argv[0], token.argv, environ);

            perror(token.argv[0]);
            // if directory doesnt exist return
            if (errno == 2)
                return;
            exit(1);
        }
        child_pid = pid;

        if (parse_result == PARSELINE_FG) {
            add_job(pid, FG, cmdline);
        }
        if (parse_result == PARSELINE_BG) {
            add_job(pid, BG, cmdline);
        }
        child_jid = job_from_pid(pid);

        if (parse_result == PARSELINE_BG) {
            sio_printf("[%d] (%d) %s\n", child_jid, child_pid, cmdline);
        }
        pid = 0;
        while (!pid && (fg_job() != 0)) {
            sigsuspend(&prev);
        }
    }
    // restore masks
    sigprocmask(SIG_SETMASK, &prev, NULL);
}

/*****************
 * Signal handlers
 *****************/

/**
 * @brief Deals with SIGCHLD signal. Properly reaps children and
 * modifies the correct data structures in the process
 *
 * Identify how the child was stopped/killed and changes the job list
 * accordingly
 */
void sigchld_handler(int sig) {

    int olderrno = errno;

    sigset_t mask_all, prev_all;

    sigfillset(&mask_all);
    int status;

    jid_t jid;

    while ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG)) > 0) {

        sigprocmask(SIG_BLOCK, &mask_all, &prev_all);

        jid = job_from_pid(pid);

        if (WIFEXITED(status)) {
            delete_job(jid);
        } else if (WIFSTOPPED(status)) {
            sio_printf("Job [%d] (%d) stopped by signal %d\n", jid, pid,
                       WSTOPSIG(status));
            job_set_state(jid, ST);
        } else if (WIFSIGNALED(status)) {
            sio_printf("Job [%d] (%d) terminated by signal %d\n", jid, pid,
                       WTERMSIG(status));
            delete_job(jid);
        } else {
            sio_printf("fuck...\n");
        }

        sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    errno = olderrno;
}

/**
 * @brief Deals with SIGINT (^C) sent from the user
 * Sends SIGINT to the foregound job in the shell as opposed to the shell itself
 * Avoids killing the shell whenever (^C) is pressed
 *
 *
 */
void sigint_handler(int sig) {
    int olderrno = errno;
    sigset_t mask, prev;
    jid_t fg;
    pid_t fg_pid;
    sigfillset(&mask);

    sigprocmask(SIG_BLOCK, &mask, &prev);
    if ((fg = fg_job()) > 0) {
        fg_pid = job_get_pid(fg);

        kill(-fg_pid, SIGINT);
    }

    sigprocmask(SIG_SETMASK, &prev, NULL);
    errno = olderrno;
}

/**
 * @brief Deals with SIGSTP (^Z) sent from the user
 * Pauses the foreground process as opposed to the shell itself
 * moves the foreground process to the "stopped" state and it can
 * later be started again
 */
void sigtstp_handler(int sig) {
    int olderrno = errno;
    sigset_t mask, prev;
    jid_t fg;
    pid_t fg_pid;
    sigfillset(&mask);

    sigprocmask(SIG_BLOCK, &mask, &prev);
    if ((fg = fg_job()) > 0) {
        fg_pid = job_get_pid(fg);

        kill(-fg_pid, SIGTSTP);
    }

    sigprocmask(SIG_SETMASK, &prev, NULL);
    errno = olderrno;
}

/**
 * @brief Attempt to clean up global resources when the program exits.
 *
 * In particular, the job list must be freed at this time, since it may
 * contain leftover buffers from existing or even deleted jobs.
 */
void cleanup(void) {
    // Signals handlers need to be removed before destroying the joblist
    Signal(SIGINT, SIG_DFL);  // Handles Ctrl-C
    Signal(SIGTSTP, SIG_DFL); // Handles Ctrl-Z
    Signal(SIGCHLD, SIG_DFL); // Handles terminated or stopped child

    destroy_job_list();
}
