#include "fss_manager.h"

void init_queue(Queuehead *q) {
    q->front= NULL;
    q->rear= NULL;
}

WatchNode* find_watch_node_by_source(WatchNode *head, const char *source_path) {
    while(head) {
        if(strcmp(head->source_path, source_path) == 0)
            return head;
        head = head->next;
    }
    return NULL;
}

void enqueue(Queuehead *q, const char *src, const char *tgt, const char *file, const char *op) {
    QueueNode *new_task = malloc(sizeof(QueueNode));
    new_task->source_dir = strdup(src);
    new_task->target_dir = strdup(tgt);
    new_task->filename = strdup(file);
    new_task->operation = strdup(op);
    new_task->next = NULL;
    if(q->rear) q->rear->next = new_task;
    else q->front = new_task;
    q->rear = new_task;
}

QueueNode *dequeue(Queuehead *q) {
    if(!q->front) return NULL;
    QueueNode *temp = q->front;
    q->front = q->front->next;
    if(!q->front) q->rear = NULL;
    return temp;
}

int is_empty(Queuehead *q) {
    if(q->front == NULL) {
        return 1;
    }
    return 0;
}

void free_QueueNode(QueueNode * task) {
    if (!task) return;
    free(task->source_dir);
    free(task->target_dir);
    free(task->filename);
    free(task->operation);
    free(task);
}

void read_and_print_pipe(int pipe_fd, int manager_log_file_fd, Worker* worker, int pid, WatchNode* head, int fd_fss_out) {
    size_t buf_size = INITIAL_BUF_SIZE;
    size_t total_read = 0;
    ssize_t bytes_read;
    char *buffer = malloc(buf_size);

    if(!buffer) {
        perror("Malloc Failed\n");
        return;
    }

    while((bytes_read = read(pipe_fd, buffer + total_read, buf_size - total_read)) > 0) {
        total_read += bytes_read;

        // If we fill the buffer, expand it
        if(total_read >= buf_size) {
            buf_size *= 2;
            char *new_buf = realloc(buffer, buf_size);
            if(!new_buf) {
                perror("Realloc Failed\n");
                free(buffer);
                return;
            }
            buffer = new_buf;
        }
    }

    if(bytes_read == -1) {
        perror("Read from worker pipe failed\n");
        free(buffer);
        return;
    }
    buffer[total_read]= '\0';

    time_t now = time(NULL);
    struct tm *time = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", time);
    
    char* operation;
    if(worker->operation_type == 1) {
        operation="FULL";
    }
    else if(worker->operation_type == 2) {
        operation="MODIFIED";
    }
    else if(worker->operation_type == 3) {
        operation="ADDED";
    }
    else  if(worker->operation_type == 4) {
        operation="DELETED";
    }
    else {
        operation="UNKNOWN";
    }

    char **errors= NULL;
    int error_count= 0;    

    char *line= strtok(buffer, "\n");
    char *details= NULL;
    char *status= NULL;;
    while(line) {
        if(strncmp(line, "STATUS: ", 8) == 0) {
            char *value = line + 8;
            status = strdup(value);
        }
        if((strncmp(line, "DETAILS: ", 9) == 0) && (worker->operation_type == 1)) {//DONT PRINT DETAILS IF WE ARENT ΙΝ FULL SYNC
            char *value = line + 9;
            details = strdup(value);
        }
        if((strncmp(line, "ERRORS:", 7) == 0) && (strcmp(status, "ERROR")==0)) {
            
            line= strtok(NULL, "\n"); //move to next line
        
            if (line == NULL || strncmp(line, "- ", 2) != 0) {
                break;
            }
            while (line && line[0] == '-') {        
                errors = realloc(errors, (error_count+ 1) * sizeof(char *));
                if(!errors) {
                    perror("Failed to realloc for errors");
                    break;
                }
        
                errors[error_count] = strdup(line);
                if(!errors[error_count]) {
                    perror("Failed to strdup error line");
                    break;
                }
                error_count++;
                line= strtok(NULL, "\n");
            }
            break;
        }
        line= strtok(NULL, "\n");
    }
    WatchNode* node=find_watch_node_by_source(head, worker->source_dir);
    node->errors= error_count;
    if(node->in_use == 0) {// WE ARE IN SYNC COMMAND, NEEDED TO DO IT LIKE THIS SO I KNOW WHEN THE COMMAND ENDS cause SYNC COMMAND demands different timestamp for second message
        node->in_use=1;
        size_t needed=strlen(timestamp)+strlen(worker->source_dir)+50+strlen(worker->target_dir);
        char* logfile_message=malloc(needed);
        int message_length=snprintf(logfile_message,needed,"%s Sync completed %s -> %s\n\n",timestamp,worker->source_dir, worker->target_dir);
        write(manager_log_file_fd, logfile_message, message_length);
        write(STDOUT_FILENO, logfile_message, message_length);
        write(fd_fss_out, logfile_message, message_length);
        free(logfile_message);
    
    }

    size_t needed= strlen(timestamp)+ strlen(worker->source_dir)+ strlen(worker->target_dir)+ strlen(operation) + 50 + strlen(status);
    char* logfile_message=malloc(needed);
    if(logfile_message == NULL) {
        perror("malloc failed");
    }

    int message_length=snprintf(logfile_message, needed, "%s [%s] [%s] [%d] [%s]\n[%s] ", timestamp, worker->source_dir, worker->target_dir, pid, operation, status);
    write(manager_log_file_fd, logfile_message, message_length);
    if((worker->operation_type == 1) && (strcmp(status, "ERROR")!=0)) {
        size_t needed_for_details= strlen(details)+3;
        char* log_details=malloc(needed_for_details);
        int log_details_length=snprintf(log_details, needed_for_details, "[%s]", details);
        write(manager_log_file_fd, log_details, log_details_length);
    }

    if(error_count ==0) {
        write(manager_log_file_fd, "\n", strlen("\n"));
    }
    for(int i = 0; i < error_count; ++i) {
        size_t len = strlen(errors[i]) + 4; //
        char *error_line = malloc(len);
        if (!error_line) {
            perror("malloc for error line failed");
            continue;
        }
    
        snprintf(error_line, len, "[%s]\n", errors[i]);
        write(manager_log_file_fd, error_line, strlen(error_line));
        free(error_line);
    }
    write(manager_log_file_fd, "\n", strlen("\n"));
    free(buffer);
    if(status)free(status);
    if(details)free(details);
    for(int i = 0; i < error_count; ++i) {
        free(errors[i]);
    }
    free(errors);
    free(logfile_message);
}

int find_free_worker_slot(int worker_limit,Worker *worker) {
    for(int i = 0; i < worker_limit; i++) {
        if(!worker[i].in_use)
            return i;
    }
    return -1;
}

int free_worker_by_pid(pid_t pid, int worker_limit,int current_workers,Worker* worker) {
    for(int i = 0; i < worker_limit; i++) {
        if(worker[i].pid == pid) {
            close(worker[i].pipe_fd[READ]);
            worker[i].pid = 0;
            worker[i].in_use = 0;
            free(worker[i].source_dir);
            free(worker[i].target_dir);
            worker[i].operation_type=0;
            current_workers--;
            return current_workers;
        }
    }
    return current_workers;
}

void add_watch_node(WatchNode **head, int wd, char *source_path, char* target_path, char* timestamp) {//Add a new watch
    WatchNode *node= malloc(sizeof(WatchNode));
    node->wd= wd;
    node->source_path= strdup(source_path);
    node->target_path= strdup(target_path);
    node->next= *head;
    node->in_use=1;//for status | cancel | sync
    node->errors=0;//error count basically
    node->last_sync= strdup(timestamp);
    *head= node;
}


char* get_watch_source_path(WatchNode *head, int wd) {//Find path by wd
    while(head) {
        if(head->wd == wd)
            return head->source_path;
        head= head->next;
    }
    return NULL;
}
char* get_watch_target_path(WatchNode *head, int wd) {//Find path by wd
    while(head) {
        if(head->wd == wd)
            return head->target_path;
        head= head->next;
    }
    return NULL;
}



void free_watch_list(WatchNode *head) {//Free watch list
    while(head) {
        WatchNode *next= head->next;
        free(head->source_path);
        free(head->target_path);
        free(head->last_sync);
        free(head);
        head= next;
    }
}

int read_from_fss_in_and_print(int fd_fss_in, int fd_fss_out, WatchNode **head, Queuehead *queue, int inotify_fd, int manager_log_file_fd ) {
    char buffer[4096];
    ssize_t total_read = 0;
    ssize_t bytes_read;

    char command[64];
    char source_path[256];
    char dest_path[256];

    while((bytes_read = read(fd_fss_in, buffer + total_read, sizeof(buffer) - total_read)) > 0) {
        total_read += bytes_read;
    }
    if(total_read >0) {//Read didnt fail
        buffer[total_read]='\0';
        if(sscanf(buffer, "%63s", command) != 1) {
        return -1;
        }
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", t);

        if(strcmp(command, "add") == 0) {
            if(sscanf(buffer, "%*s %255s %255s", source_path, dest_path) != 2) { //skip the first word (already scanned)
                return -1;
            }
            WatchNode* node=find_watch_node_by_source(*head, source_path);
            if(node !=NULL) {//SOURCE ALREADY IN QUEUE
                size_t needed=strlen(timestamp)+strlen(source_path)+50;
                char* logfile_message3=malloc(needed);
                if(logfile_message3==NULL){
                    perror("Malloc failed");
                    return -1;
                }
                int message_length=snprintf(logfile_message3, needed, "%s Already in queue: %s\n",timestamp, source_path);
                write(STDOUT_FILENO, logfile_message3, message_length);
                return 0;
            }
            int wd_directory = inotify_add_watch(inotify_fd, source_path, IN_DELETE_SELF | IN_MODIFY | IN_CREATE | IN_DELETE);
            if (wd_directory == -1) {
                write(STDOUT_FILENO, message12 , strlen(message12));
                return -1;
            } else {
                add_watch_node(head, wd_directory, source_path, dest_path,timestamp);
            }

            enqueue(queue, source_path, dest_path, "ALL", "FULL");

            size_t needed1= strlen(timestamp)+ strlen(source_path) + strlen(dest_path) + 50;
            char* logfile_message=malloc(needed1);
            if(logfile_message == NULL) {
                perror("malloc failed");
                return -1;
            }
            int message_length=snprintf(logfile_message, needed1, "%s Added directory: %s -> %s\n", timestamp, source_path, dest_path);
            write(manager_log_file_fd, logfile_message, message_length);
            write(STDOUT_FILENO, logfile_message, message_length);
            write(fd_fss_out, logfile_message, message_length);
            free(logfile_message);


            size_t needed2= strlen(timestamp)+ strlen(source_path)+ 50;
            char* logfile_message_2=malloc(needed2);
            if(logfile_message_2 == NULL) {
                perror("malloc failed");
            }
            message_length=snprintf(logfile_message_2, needed2, "%s Monitoring started for %s\n\n", timestamp, source_path);
            write(manager_log_file_fd, logfile_message_2, message_length);
            write(STDOUT_FILENO, logfile_message_2, message_length);
            write(fd_fss_out, logfile_message_2, message_length);
            free(logfile_message_2);

            return 1;
        }
    
        if(strcmp(command, "status") == 0) {
            if(sscanf(buffer, "%*s %255s", source_path) != 1) {
                // fprintf(stderr, "Invalid status command format. Usage: status <directory>\n");
                return -1;
            }
            WatchNode* node=find_watch_node_by_source(*head, source_path);
            if(node == NULL) {//NOT FOUND
                size_t needed=strlen(timestamp)+strlen(source_path)+50;
                char* logfile_message3=malloc(needed);
                if(logfile_message3==NULL){
                    perror("Malloc failed");
                    return -1;
                }
                int message_length=snprintf(logfile_message3, needed, "%s Directory not monitored: %s\n",timestamp, source_path);
                write(STDOUT_FILENO, logfile_message3, message_length);
                write(fd_fss_out, logfile_message3, message_length);
                free(logfile_message3);
                return 0;
            }

            size_t needed=strlen(timestamp)+strlen(node->last_sync)+ 2*strlen(source_path)+ strlen(node->target_path)+100;
            char* logfile_message4=malloc(needed);
            int message_length = snprintf(logfile_message4, needed, 
                "%s Status requested for %s\n"
                "Directory: %s\n"
                "Target: %s\n"
                "Last Sync: %s\n"
                "Errors: %d\n"
                "Status: %s\n\n",
                timestamp, 
                source_path, 
                node->source_path, 
                node->target_path, 
                node->last_sync, 
                node->errors, 
                node->in_use ? "ACTIVE" : "NOT ACTIVE"
            );
            write(STDOUT_FILENO,logfile_message4, message_length);
            write(fd_fss_out,logfile_message4, message_length);
    
            return 2;
            
        }
        if(strcmp(command, "cancel") == 0) {
            if(sscanf(buffer, "%*s %255s", source_path) != 1) {
                // fprintf(stderr, "Invalid cancel command format. Usage: cancel <source>\n");
                return -1;
            }
            WatchNode* node=find_watch_node_by_source(*head, source_path);
            if(node==NULL || !(node->in_use)) { //1 in use, 0 not in use
                size_t needed=strlen(timestamp)+50+strlen(source_path);
                char* logfile_message=malloc(needed);
                int message_length=snprintf(logfile_message, needed,"%s Directory not monitored %s\n\n",timestamp,source_path);
                write(STDOUT_FILENO,logfile_message, message_length);
                write(fd_fss_out,logfile_message, message_length);
                free(logfile_message);
                return 0;
            }
            if(node->in_use) {
                //Remove watch from inotify
                if (inotify_rm_watch(inotify_fd, node->wd) == -1) {
                    perror("inotify_rm_watch failed\n");
                    return -1;
                }
                node->wd=-1;
                node->in_use=0;
                size_t needed= strlen(timestamp) + 50 +strlen(source_path);
                char* logfile_message=malloc(needed);
                int message_length=snprintf(logfile_message,needed,"%s Monitoring stopped for %s\n\n",timestamp,source_path);
                write(manager_log_file_fd,logfile_message, message_length);
                write(STDOUT_FILENO,logfile_message, message_length);
                write(fd_fss_out,logfile_message,message_length);
                free(logfile_message);
            }   
            return 3;
            
        }
        if(strcmp(command, "sync") == 0) { 
            if(sscanf(buffer, "%*s %255s", source_path) != 1) {
                return -1;
            }
            WatchNode* node=find_watch_node_by_source(*head, source_path);
            if(node->in_use) { //Already Monitored
                size_t needed=strlen(timestamp)+50+strlen(source_path);
                char* logfile_message=malloc(needed);
                int message_length=snprintf(logfile_message, needed,"%s Sync already in progress %s\n\n",timestamp,source_path);
                write(STDOUT_FILENO,logfile_message, message_length);
                write(fd_fss_out,logfile_message, message_length);
                free(logfile_message);
                return 0;
            }
            else if(!(node->in_use)) {
                //Then run FULL SYNC and because node->in_use will be 0 ill know in exect reprot print to only print the Sync completed
                size_t needed=strlen(timestamp)+50+strlen(source_path)+strlen(node->target_path);
                char* logfile_message=malloc(needed);
                int message_length=snprintf(logfile_message, needed,"%s Syncing directory %s -> %s\n",timestamp,source_path,node->target_path);
                write(STDOUT_FILENO,logfile_message, message_length);
                write(manager_log_file_fd,logfile_message,message_length);
                write(fd_fss_out,logfile_message,message_length);
                free(logfile_message);
                enqueue(queue,source_path,node->target_path,"ALL","FULL");
            }
            return 4;
            
        }
        if(strcmp(command, "shutdown") == 0) {
            size_t needed=strlen(timestamp)+50;
            char* logfile_message=malloc(needed);
            int message_length=snprintf(logfile_message, needed,"%s Shutting down manager...\n",timestamp);
            write(STDOUT_FILENO,logfile_message, message_length);
            write(fd_fss_out,logfile_message,message_length);
            return 5;
        }

        return 0;
    }
    return -1;
}