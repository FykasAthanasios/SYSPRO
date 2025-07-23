#include "lib.h"

extern volatile sig_atomic_t child_finished;
void sigchld_handler(int signo);
