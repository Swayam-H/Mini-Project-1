#include "signals.h"
#include "jobs.h"
#include <signal.h>
#include <stddef.h>

void sigint_handler(int sig) {
    (void)sig;
}

void sigtstp_handler(int sig) {
    (void)sig;
}

void install_shell_signals(void) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);

    signal(SIGTTOU, SIG_IGN);

    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);
}
