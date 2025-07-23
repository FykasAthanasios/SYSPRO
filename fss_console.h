#define _GNU_SOURCE
#include "Errors.h"
#include "signals.h"

#define READ 0
#define WRITE 1

int extract_info_from_input(char* buffer, int manager_logfile_fd);//reads from stdin,
// parse the info and then prints the correct message to the console logfile 