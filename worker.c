#include "worker.h"

int main(int argc, char* argv[]) {

    //Stuff for EXEC_REPORT
    int files_copied=0, files_skipped=0;
    ErrorNode *error_list = NULL; //Simple linked list for saving errors before printing them (dont forget to free it)

    if(argc!= 5) { //Do i need to include this on the exec report???
        add_error(&error_list, "Not enough arguments", argv[1], 1);
        print_exec_report(&error_list, 2 , files_skipped, files_copied);
        exit(1);
    }
    
    char* source_dir = argv[1];
    char* target_dir = argv[2];
    char* filename = argv[3];
    if (strcmp(filename, "__DIR__") == 0) {//Special case for filename empty , have it as __DIR__
        filename = "";
    }
    char* operation = argv[4];
    if (strcmp(operation, "DELETED") == 0) {//directory deleted, if directory is not empty 
        if (rmdir(target_dir) == -1) {//then rmdir wont delete properly and will return error
            add_error(&error_list,strerror(errno), target_dir , 1);
            print_exec_report(&error_list, 2, files_skipped , files_copied);
            return 1;
        }
        print_exec_report(&error_list, 0, files_skipped , files_copied);
        return 0;
    }
    bool SUCCESS=false, ERROR=false;//flags for operation

    DIR *dir_pointer = opendir(source_dir);
    if(dir_pointer == NULL) {
        
        add_error(&error_list,strerror(errno), source_dir , 1);
        print_exec_report(&error_list, 2, files_skipped, files_copied);
        free_errors(error_list);
        exit(1);
    }
    if (strcmp(operation, "FULL") == 0 && strcmp(filename, "ALL") == 0) {//FULL SYNC
        if(mkdir_p(target_dir, &error_list)== -1) {
            ERROR=true;
            add_error(&error_list,strerror(errno), target_dir , 1);
            print_exec_report(&error_list, 2, files_skipped,files_copied );
            return 1;
        }
        struct dirent* dir_comp;
    
        while ((dir_comp = readdir(dir_pointer)) != NULL) {
            if (dir_comp->d_type != DT_REG) continue;
    
            char src_path[PATH_MAX];
            char dst_path[PATH_MAX];
            snprintf(src_path, sizeof(src_path), "%s/%s", source_dir, dir_comp->d_name);
            snprintf(dst_path, sizeof(dst_path), "%s/%s", target_dir, dir_comp->d_name);
    
            if (copy_file(&error_list,src_path, dst_path) == -1) {
                files_skipped++;
                ERROR = true;
            } else {
                files_copied++;
                SUCCESS = true;
            }
        }
    
        closedir(dir_pointer);
        
        if (ERROR && SUCCESS) {
            //PARTIAL CASE
            print_exec_report(&error_list, 1, files_skipped, files_copied);
        } else if (ERROR && !SUCCESS) {
            print_exec_report(&error_list, 2, files_skipped, files_copied);
        } else if (!ERROR && SUCCESS) {
            print_exec_report(&error_list, 0, files_skipped, files_copied);
        }
        if (!ERROR && !SUCCESS) {
            print_exec_report(&error_list, 0, files_skipped, files_copied);
        }
        free_errors(error_list);
        return 0;
    }
    else if (strcmp(operation, "ADDED") == 0 && (strcmp(filename,"") !=0)) {//file added not directory
        char src_path[PATH_MAX];
        char dst_path[PATH_MAX];
        snprintf(src_path, sizeof(src_path), "%s/%s", source_dir, filename);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", target_dir, filename);

        int fd= open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            add_error(&error_list,strerror(errno), target_dir , 0);
            files_skipped++;
            print_exec_report(&error_list, 2, files_skipped , files_copied);
            return 1;
        }
        else  {
            files_copied++;
            print_exec_report(&error_list, 0, files_skipped, files_copied);
            return 0;
        }   
    }
    else if (strcmp(operation, "MODIFIED") == 0 && (strcmp(filename,"") !=0)) {// File modified
        char src_path[PATH_MAX];
        char dst_path[PATH_MAX];
        snprintf(src_path, sizeof(src_path), "%s/%s", source_dir, filename);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", target_dir, filename);
        int fd= open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            add_error(&error_list,strerror(errno), target_dir , 0);
            files_skipped++;
            print_exec_report(&error_list, 2, files_skipped , files_copied);
            return 1;
        }
        else  {
            if (copy_file(&error_list ,src_path, dst_path) == -1) {
                ERROR = true;
                files_skipped++;
                print_exec_report(&error_list, 2, files_skipped , files_copied);
                return 1;
            } else {
                SUCCESS = true;
                files_copied++;
                print_exec_report(&error_list, 0, files_skipped, files_copied);
                return 0;
            }   
        }   
    }
    else if (strcmp(operation, "DELETED") == 0 && (strcmp(filename,"") !=0)) {// File deleted
        char src_path[PATH_MAX];
        char dst_path[PATH_MAX];
        snprintf(src_path, sizeof(src_path), "%s/%s", source_dir, filename);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", target_dir, filename);
        if (unlink(dst_path) == -1) {
            add_error(&error_list, strerror(errno), dst_path, 0);
            files_skipped++;
            print_exec_report(&error_list, 2, files_skipped, files_copied);
            return 0;
        }
        print_exec_report(&error_list, 0, files_skipped , files_copied);
        return 0;
    }    
    return 0;
}

//Lets say that the standard call ofr this program is ./worker source_dir target_dir filename operation
