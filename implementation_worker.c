#include "worker.h"

int mkdir_p(const char *path,ErrorNode **head) {//Creates parent directories recursively
    char temp[PATH_MAX];
    snprintf(temp, sizeof(temp), "%s", path);

    for(char *p = temp + 1; *p; p++) {
        if(*p == '/') {
            *p = '\0';
            if(mkdir(temp, 0755) == -1 && errno != EEXIST) {
                add_error(head, strerror(errno), temp, 1);
                return -1;
            }
            *p = '/';   
        }
    }

    if(mkdir(temp, 0755) == -1 && errno != EEXIST) {
        return -1;
    }

    return 0;
}


int copy_file(ErrorNode **head,char *src_path, char *dst_path) {
    int source_fd = open(src_path, O_RDONLY);
    if(source_fd == -1) {
        add_error(head,strerror(errno),src_path, 0);
        return -1;
    }

    int destination_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(destination_fd == -1) {
        add_error(head,strerror(errno), dst_path, 0);
        close(source_fd);//If open dest fails, close src fd (cause this block of code will be execute after the first one is already passed)
        return -1;
    }

    char buffer[BUF_SIZE];
    ssize_t bytes_read;
    while((bytes_read = read(source_fd, buffer, BUF_SIZE)) > 0) {
        if(write(destination_fd, buffer, bytes_read) != bytes_read) {
            add_error(head,strerror(errno), dst_path , 0);
            close(source_fd);
            close(destination_fd);
            return -1;
        }
    }

    close(source_fd);
    close(destination_fd);
    return 0;
}

void print_exec_report(ErrorNode **head, int status, int files_skipped, int files_copied) {
    char msg[1024];
    int length=0;
    write(STDOUT_FILENO, "EXEC_REPORT_START\n", strlen("EXEC_REPORT_START\n"));

    if(status == 0) {
        write(STDOUT_FILENO, "STATUS: SUCCESS\n", strlen("STATUS: SUCCESS\n"));
    } else if(status == 1) {
        write(STDOUT_FILENO, "STATUS: PARTIAL\n", strlen("STATUS: PARTIAL\n"));
    } else if(status == 2) {
        write(STDOUT_FILENO, "STATUS: ERROR\n", strlen("STATUS: ERROR\n"));
    }

    length = snprintf(msg, sizeof(msg),"DETAILS: %d files copied, %d skipped\n", files_copied, files_skipped);
    write(STDOUT_FILENO, msg, length);

    if(*head) {
        print_errors(*head);
    } else {
        write(STDOUT_FILENO, "ERRORS: None\n", 13);
    }

    write(STDOUT_FILENO, "EXEC_REPORT_END\n\n", 17);
}


void add_error(ErrorNode **head, char *msg, char* filename, int flag) { //If flag=1 then its a directory if 0 its a file
    ErrorNode *new_node = malloc(sizeof(ErrorNode));
    if(!new_node) {
        perror("Failed to allocate memory for new error");
        exit(1);
    }
    new_node->flag= flag;
    new_node->next = NULL;
    new_node->message =(char*) malloc((strlen(msg) + 1)*sizeof(char));
    if(!new_node->message) {
        perror("malloc failed");
        exit(1);
    }
    strcpy(new_node->message, msg);

    new_node->filename = (char*)malloc((strlen(filename) + 1)*sizeof(char));
    if(!new_node->filename) {
        perror("malloc failed");
        exit(1);
    }
    strcpy(new_node->filename, filename);

    if(*head == NULL) {
        *head = new_node;
    }else {
        ErrorNode *current = *head;
        while(current->next != NULL) { 
            current = current->next;
        }
        current->next = new_node;
    }
}

void print_errors(const ErrorNode *head) {
    char msg[1024]; //Protimhsa static gia logous taxuththas alla kai epeidh apla kanei to print kai h metablhth xanete otan teleivsei to function

    write(STDOUT_FILENO, "ERRORS:\n", 8);

    while(head != NULL) {
        char *type = head->flag == 1 ? "- DIR  " : "- FILE ";
        write(STDOUT_FILENO, type, strlen(type));

        int length = snprintf(msg, sizeof(msg), "%s: %s\n",head->filename, head->message);
        write(STDOUT_FILENO, msg, length);

        head = head->next;
    }
}

void free_errors(ErrorNode *head) {
    while(head != NULL) {
        ErrorNode *temp = head;
        head = head->next;

        free(temp->message);
        free(temp->filename);
        free(temp);
    }
}