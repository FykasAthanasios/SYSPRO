#include "fss_manager.h"

volatile sig_atomic_t child_finished = 0;

void sigchld_handler(int signo) {
    (void)signo;
    child_finished = 1;
}

int main(int argc, char* argv[]) {
    if(argc!=7) {
        write(STDERR_FILENO, message3, strlen(message3));
        exit(1);
    }

    //setup handler
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    int shutdown_flag=0;

    char* manager_logfile = argv[2];
    char* config_file = argv[4];
    int worker_limit = atoi(argv[6]);

    if(worker_limit <= 0){
        worker_limit=DEFAULT_WORKER_NUMBER;
    }


    Worker* workers = malloc(worker_limit * sizeof(Worker));
    for(int i=0; i< worker_limit ; i++) {
        workers[i].pid=0;
        workers[i].in_use=0;// 0 not in use, 1 in use
    }         
    int current_workers = 0;

    WatchNode *watch_head = NULL;// Ininitialize empty linked list for the watches list

    
    //Create if not already the manager log file and config log files
    int fd_manager_logfile = open(manager_logfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_manager_logfile == -1) {
        write(STDERR_FILENO, message1, strlen(message1));
        exit(1);
    }

    int fd_config_file = open(config_file, O_RDONLY);
    if (fd_config_file == -1 ) {
        write(STDERR_FILENO, message1, strlen(message1));
        exit(1);
    }

    const char *fss_in = "fss_in";
    const char *fss_out = "fss_out";

    //make sure they are deleted from previous use of program
    unlink(fss_in);
    unlink(fss_out);

    //Create the named pipes for communcation with  console
    if (mkfifo(fss_in, 0666) == -1 || mkfifo(fss_out, 0666) == -1) {
        write(STDERR_FILENO, message6, strlen(message6));
        exit(1);
    }

    FILE *filePtr = fdopen(fd_config_file, "r");
    if (filePtr == NULL) {
        write(STDERR_FILENO, "fdopen failed\n", 15);
        exit(1);
    }

    int fss_in_fd = open(fss_in, O_RDONLY | O_NONBLOCK);
    if (fss_in_fd == -1) {
        perror("open fss_in");
        exit(1);
    }

    int fss_out_fd = open(fss_out, O_RDWR | O_NONBLOCK); //OPEN FSS_OUT WHEN I KNOW CONSOLE IS READING FROM THE OTHER END
    if (fss_out_fd == -1) {
        perror("open fss_out");
        exit(1);
    }

    //Create the inotify instance
    int inotify_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_fd == -1) {
        perror("inotify_init");
        exit(1);
    }

    //Preparing the loop to read the contents of config log and start copying in workers
    char *source_dir = malloc(50 * sizeof(char));
    char *target_dir = malloc(50 * sizeof(char));
    char line[MAX_LINE_LENGTH];
    // int operation_flag=1;//FULL SYNC = 1, MODIFIED= 2, ADDED= 3 , DELETED=4 
    
    Queuehead* queue=malloc(sizeof(Queuehead));
    init_queue(queue);
    while (fgets(line, sizeof(line), filePtr)) {    
        if (sscanf(line, "%49s %49s", source_dir, target_dir) != 2)continue;
        time_t remaining_tasks = time(NULL);
        struct tm* t = localtime(&remaining_tasks);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", t);
        char message[256];
        int message_length = snprintf(message, sizeof(message), "%s Added directory: %s -> %s \n", timestamp, source_dir,target_dir);
        write(fss_out_fd, message, message_length);
        write(STDOUT_FILENO, message, message_length);
        enqueue(queue, source_dir, target_dir, "ALL", "FULL");
    }
    while (is_empty(queue)!= 1) {//while queue not empty
        
        QueueNode* pair=dequeue(queue);
        //Before Continuing with the setup, we need to set our watches on the source directories
        
        //Set watch on directory
        int wd_directory = inotify_add_watch(inotify_fd, pair->source_dir, IN_DELETE_SELF | IN_MODIFY | IN_CREATE | IN_DELETE);
        if (wd_directory == -1) {
            write(STDOUT_FILENO, message12 , strlen(message12));
            exit(1);;
        } else {
            time_t now = time(NULL);
            struct tm* t = localtime(&now);
            char timestamp[32];
            strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", t);
            char message[256];
            int message_length = snprintf(message, sizeof(message), "%s Monitoring started for %s\n", timestamp, pair->source_dir);
            write(fss_out_fd, message, message_length);
            write(STDOUT_FILENO, message, message_length);
            add_watch_node(&watch_head, wd_directory, pair->source_dir, pair->target_dir, timestamp);
        }

        int status;
        pid_t finished_pid;
        int i;

        //If im at worker limit break from the loop and start trying to read from console
        if (current_workers >= worker_limit) {
            break;
        }
        while (((finished_pid = waitpid(-1, &status, WNOHANG)) > 0)&& (child_finished== 1))  {
            child_finished=0;//Reset for handler
            for (i = 0; i < worker_limit; i++) {
                if (workers[i].pid == finished_pid && workers[i].in_use) {
                    break;
                }
            }
            read_and_print_pipe(workers[i].pipe_fd[READ],fd_manager_logfile, &workers[i] , finished_pid, watch_head,fss_out_fd);
            current_workers=free_worker_by_pid(finished_pid,worker_limit,current_workers,workers);
        } 
        

        int idx = find_free_worker_slot(worker_limit, workers); /*After i waited for any zombie
        processes and made sure that i have available workers, im finding the first available worker 
        and returning index so i can fork*/
        if ( idx == -1) {
            write(STDERR_FILENO, "No available worker slot\n", 26);
            break;
        }
        
        if (pipe(workers[idx].pipe_fd) == -1) {
            perror("pipe failed");
            break;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            break;
        }

        if (pid == 0) {  //IM IN CHILD
            //Close file cause not needed anymore in child process
            fclose(filePtr);
            close(workers[idx].pipe_fd[READ]);  //Close Read end for Child Process
            dup2(workers[idx].pipe_fd[WRITE], STDOUT_FILENO); //Redirect stdout to pipe
            close(workers[idx].pipe_fd[WRITE]); //close after dup2
            execl("./worker", "./worker", pair->source_dir, pair->target_dir, pair->filename, pair->operation, (char *)NULL);
            perror("execl failed");
            exit(1);
        } else {    //IM IN PARENT
            //Update the workers pid and in use status first
            close(workers[idx].pipe_fd[WRITE]);  //Close Write end for Parent Process
            workers[idx].pid = pid;
            workers[idx].in_use = 1;
            workers[idx].operation_type=1;
            workers[idx].source_dir = strdup(pair->source_dir);
            workers[idx].target_dir = strdup(pair->target_dir);
            free_QueueNode(pair);   
            current_workers++;
        }
    }
    int epoll_fd = epoll_create1(0);
    if(epoll_fd == -1) {
        perror("epoll_create1");
        exit(1);
    }

    struct epoll_event event, events[2];

    //add inotify_fd to epoll
    event.events = EPOLLIN;
    event.data.fd = inotify_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inotify_fd, &event) == -1) {
        perror("epoll inotify_fd");
        exit(1);
    }

    //add fss_in_fd to epoll
    event.events = EPOLLIN;
    event.data.fd = fss_in_fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fss_in_fd, &event) == -1) {
        perror("epoll fss_in_fd");
        exit(1);
    }

    char buffer[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));

    while(1) {
        int n = epoll_wait(epoll_fd, events, 2, 0);
        if(n == -1) {
            perror("epoll_wait");
            break;
        }

        //check for finished children
        int status;
        pid_t finished_pid;
        while((finished_pid = waitpid(-1, &status, WNOHANG)) > 0 && child_finished == 1) {
            child_finished = 0;
            for(int i = 0; i < worker_limit; i++) {
                if(workers[i].pid == finished_pid && workers[i].in_use) {
                    read_and_print_pipe(workers[i].pipe_fd[READ], fd_manager_logfile, &workers[i], finished_pid, watch_head,fss_out_fd);
                    current_workers = free_worker_by_pid(finished_pid, worker_limit, current_workers, workers);
                    break;
                }
            }
        }
        if(shutdown_flag == 0) {//if shutdown flag==1  then stop reading any more commands and exit
            for(int i = 0; i < n; i++) {
                if(events[i].data.fd == inotify_fd) {
                    ssize_t len = read(inotify_fd, buffer, sizeof(buffer));
                    if(len > 0) {
                        for(char *ptr = buffer; ptr < buffer + len; ptr += sizeof(struct inotify_event) + ((struct inotify_event *)ptr)->len) {
                            struct inotify_event *event = (struct inotify_event *)ptr;
                            char *src = get_watch_source_path(watch_head, event->wd);
                            char *target= get_watch_target_path(watch_head, event->wd);
                            if(event->mask & IN_MODIFY) {
                                enqueue(queue, src, target, event->name, "MODIFIED");
                            }
                            if(event->mask & IN_CREATE) {
                                enqueue(queue, src, target, event->name, "ADDED");
                            }
                            if(event->mask & IN_DELETE) {
                                enqueue(queue, src, target, event->name, "DELETED");
                            }
                            if(event->mask & IN_DELETE_SELF) {
                                enqueue(queue, src, target, "__DIR__", "DELETED");
                            }
                        }
                    }
                }else if (events[i].data.fd == fss_in_fd) {
                    if(read_from_fss_in_and_print(fss_in_fd,fss_out_fd, &watch_head, queue, inotify_fd, fd_manager_logfile)==5){
                        shutdown_flag=1;
                        //Delete all watches
                        WatchNode* temp = watch_head;
                        while (temp) {
                            if (temp->in_use && temp->wd != -1) {
                                if (inotify_rm_watch(inotify_fd, temp->wd) == -1) {
                                    perror("inotify_rm_watch failed during shutdown");
                                }
                                temp->wd = -1;
                                temp->in_use = 0;
                            }
                            temp = temp->next;
                        }
                        time_t remaining_tasks = time(NULL);
                        struct tm* t = localtime(&remaining_tasks);
                        char timestamp[32];
                        strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", t);

                        char message[256];
                        int message_length = snprintf(message, sizeof(message), "%s Processing remaining queued tasks.\n", timestamp);
                        write(fss_out_fd, message, message_length);
                        write(STDOUT_FILENO, message, message_length);
                    }
                }
            }
        }
        if(is_empty(queue) && shutdown_flag==1) {
            time_t now = time(NULL);
            struct tm* t = localtime(&now);
            char timestamp[32];
            strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", t);
            
            
            int status;
            pid_t finished_pid;
            //wait all children before exiting
            while((finished_pid = wait(&status)) > 0) {
                for(int i = 0; i < worker_limit; i++) {
                    if(workers[i].pid == finished_pid && workers[i].in_use) {
                        read_and_print_pipe(workers[i].pipe_fd[READ], fd_manager_logfile, &workers[i], finished_pid, watch_head, fss_out_fd);
                        current_workers = free_worker_by_pid(finished_pid, worker_limit, current_workers, workers);
                        break;
                    }
                }
            }

            char final_message[256];
            int msg_len = snprintf(final_message, sizeof(final_message), "%s Manager shutdown complete.\n", timestamp);
            

            char waiting_workers[256];
            int message_length=snprintf(waiting_workers, sizeof(waiting_workers),"%s Waiting for all active workers to finish.\n", timestamp);
            
            write(fss_out_fd, waiting_workers, message_length);
            write(fss_out_fd, final_message, msg_len);
           
            write(STDOUT_FILENO, waiting_workers, message_length);
            write(STDOUT_FILENO, final_message, msg_len);
            break;
        }
        // Try to spawn workers
        while (!is_empty(queue) && current_workers < worker_limit) {
            QueueNode *task = dequeue(queue);
            int idx = find_free_worker_slot(worker_limit, workers);
            if(idx == -1) {
                enqueue(queue, task->source_dir, task->target_dir, task->filename, task->operation);
                free_QueueNode(task);
                break;
            }

            if(pipe(workers[idx].pipe_fd) == -1) {
                perror("pipe");
                free_QueueNode(task);
                continue;
            }

            pid_t pid = fork();
            if(pid == -1) {
                perror("fork");
                free_QueueNode(task);
                continue;
            }

            if(pid == 0) {
                close(workers[idx].pipe_fd[READ]);
                dup2(workers[idx].pipe_fd[WRITE], STDOUT_FILENO);
                close(workers[idx].pipe_fd[WRITE]);
                execl("./worker", "./worker", task->source_dir, task->target_dir, task->filename, task->operation, (char *)NULL);
                perror("execl");
                exit(1);
            } else {
                close(workers[idx].pipe_fd[WRITE]);
                workers[idx].pid = pid;
                workers[idx].in_use = 1;
                if(strcmp(task->operation,"FULL")== 0) {
                    workers[idx].operation_type=1;
                }
                else if(strcmp(task->operation,"MODIFIED")==0) {
                    workers[idx].operation_type=2;
                }
                else if(strcmp(task->operation,"ADDED")==0) {
                    workers[idx].operation_type=3;
                }
                else if(strcmp(task->operation,"DELETED")==0) {
                    workers[idx].operation_type=4;
                }
                workers[idx].source_dir = strdup(task->source_dir);
                workers[idx].target_dir = strdup(task->target_dir);
                current_workers++;
                free_QueueNode(task);
            }
        }
    }
    
    //Free functions and unlink the named pipes used for communication with console
    //Unlinking of fss_out will be completed in the console
    free(queue);
    free(workers);
    free_watch_list(watch_head);
    fclose(filePtr);
    free(source_dir);
    free(target_dir);
    unlink("fss_in");
    return 0;
}
