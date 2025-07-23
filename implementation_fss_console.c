#include "fss_console.h"

int extract_info_from_input(char* buffer, int console_logfile_fd) {
    if (!buffer) return -1;
    while(*buffer == ' ') buffer++;//skip blank spaces

    char command[64];
    char source_path[256];
    char dest_path[256];

    if(sscanf(buffer, "%63s", command) != 1) {
        return -1;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", t);

    if(strcmp(command, "add") == 0) {
        if(sscanf(buffer, "%*s %255s %255s", source_path, dest_path) != 2) { //skip the first word (already scanned)
            fprintf(stderr, "Invalid 'add' command format. Usage: add <source> <target>\n");
            return -1;
        }

        char message[600];
        snprintf(message, sizeof(message), "%s Command add %s -> %s\n", timestamp, source_path, dest_path);
        write(console_logfile_fd,message, strlen(message));
        return 1;
    } 

    if(strcmp(command, "status") == 0) {
        if(sscanf(buffer, "%*s %255s", source_path) != 1) {
            fprintf(stderr, "Invalid status command format. Usage: status <directory>\n");
            return -1;
        }
    
        char message[600];
        snprintf(message, sizeof(message), "%s Command status %s\n", timestamp, source_path);
        write(console_logfile_fd,message, strlen(message));
        return 2;
    }
    if(strcmp(command, "cancel") == 0) {
        if(sscanf(buffer, "%*s %255s", source_path) != 1) {
            fprintf(stderr, "Invalid cancel command format. Usage: cancel <source>\n");
            return -1;
        }
        char message[600];
        snprintf(message, sizeof(message), "%s Command cancel %s\n", timestamp, source_path);
        write(console_logfile_fd,message, strlen(message));
        return 3;
    }
    if(strcmp(command, "sync") == 0) { 
        if(sscanf(buffer, "%*s %255s", source_path) != 1) {
            fprintf(stderr, "Invalid sync command format. Usage: cancel <directory>\n");
            return -1;
        }
        char message[600];
        snprintf(message, sizeof(message), "%s Command sync %s\n", timestamp, source_path);
        write(console_logfile_fd,message, strlen(message));
        return 4;
    }
    if(strcmp(command, "shutdown") == 0) {
        char message[600];
        snprintf(message, sizeof(message), "%s Command shutdown\n",timestamp);
        write(console_logfile_fd,message, strlen(message));
        return 5;
    } 
    fprintf(stderr, "Invalid input command\n");
    return -1;
}