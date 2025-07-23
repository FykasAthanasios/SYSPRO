#define _GNU_SOURCE
#include "Errors.h"
#define BUF_SIZE 4096

//struct for error list
typedef struct ErrorNode {
    char *message;
    struct ErrorNode *next;
    char* filename;
    int flag;//flag if its a directory or a file 1 for DIR ,0 for FILE
} ErrorNode;

int mkdir_p(const char *path, ErrorNode **head);//recursive mkdir to create target directory and its parent directories
int copy_file(ErrorNode **head, char *src_path, char *dst_path);//copies the files contents to another
void print_errors(const ErrorNode *head);//prints error list
void print_exec_report(ErrorNode **head, int status, int files_skipped, int files_copied);//prints exec report based on status.. etc
void add_error(ErrorNode **head, char *msg, char* filename, int flag);//adds an error to the error list
void free_errors(ErrorNode *head);//frees the error list that saves in a buffer all the errors for exec reprot