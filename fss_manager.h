#define _GNU_SOURCE
#include "Errors.h"
#include "signals.h"

#define DEFAULT_WORKER_NUMBER 5
#define MAX_LINE_LENGTH 128
#define READ 0
#define WRITE 1
#define INITIAL_BUF_SIZE 1024

//Worker struct, basically for the mapping child pid- pipe
typedef struct {
    pid_t pid;
    int pipe_fd[2];  // [0] = read, [1] = write
    int in_use;
    char* source_dir;
    char* target_dir;
    int operation_type; //1 = FULL SYNC, 2= MODIFIED, 3 ADDED, 4 DELETED
} Worker;

//Struct for inotify watches
typedef struct WatchNode {
    int wd;
    char *source_path;
    char *target_path;
    int in_use;
    int errors;
    char *last_sync;
    struct WatchNode *next;
} WatchNode;

//Queue for incoming tasks
typedef struct QueueNode {
    char *source_dir;
    char *target_dir;
    char *filename;
    char *operation; // "ALL", "FULL", etc.
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *front;
    QueueNode *rear;
} Queuehead;

//Queue stuct (add to mem sync) functions
void init_queue(Queuehead *q);//Initializes queue
void enqueue(Queuehead *q, const char *src, const char *tgt, const char *file, const char *op);//Addes to queue a task
QueueNode *dequeue(Queuehead *q);//Dequeues a task
int is_empty(Queuehead *q);//Checks if queue is empty(mostly for entering the create workers loop)
void free_QueueNode(QueueNode *QueueNode);//free function for freeing the queue

//Worker list which basically maps worker pid with pipe, its being implemented so i have max 
//worker limit processes running at a time
int find_free_worker_slot(int worker_limit,Worker *worker);//finds free worker slot in the (worker limit) capacity list, if not return -1
int free_worker_by_pid(pid_t pid, int worker_limit,int current_workers,Worker* worker);//function to free worker struct from the workers list


//List with all inotify watches
void add_watch_node(WatchNode **head, int wd,  char *source_path, char* target_path, char* timestamp);//Adds the inotify watch details to the list(wd , source_dir etc)
char* get_watch_source_path(WatchNode *head, int wd);//Lookup specific wd and return source_path
char* get_watch_target_path(WatchNode *head, int wd);//Lookup specific wd and return dest_path
void free_watch_list(WatchNode *head);//free function for the watch list
WatchNode* find_watch_node_by_source(WatchNode *head, const char *source_path);//Lookup specific source_path and return watch struct

//Parsing, Reading, Printing functions

//Reads from the worker pipes, parses all their info(DETAILS, STATUS,
// ERRORS..etc) and then prints accordingly to the log files or the screen
void read_and_print_pipe(int pipe_fd, int manager_log_file_fd, Worker* worker, int pid, WatchNode* head, int fd_fss_out);

//Reads commands from fss_in, parses them and enqueues the correct tasks, or does as comamnded(shutdown, status), return value used for tracking shutdown command
int read_from_fss_in_and_print(int fd_fss_in, int fd_fss_out, WatchNode **head, Queuehead *queue, int inotify_fd,int manager_log_file_fd);

