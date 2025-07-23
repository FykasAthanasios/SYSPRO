#include "fss_console.h"

int main(int argc, char* argv[]) {

    if(argc != 3) {
        perror("Correct input ./fss_consol -l <console-logfile>\n");
        exit(1);
    }

    char* console_logfile=argv[2];
    int fd_console_logfile= open(console_logfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int fd_fss_in=open("fss_in",O_WRONLY | O_NONBLOCK);
    if(fd_fss_in == -1) {
        perror("Failed to open fss_in");
        exit(1);
    }
    int fd_fss_out= open("fss_out",O_RDONLY | O_NONBLOCK);
    if(fd_fss_out == -1) {
        perror("Failed to open fss_out");
        exit(1);
    }

    char input[600];
    char output_buffer [600];    
    fd_set read_fds;
   

    while(1) {
        
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);  
        FD_SET(fd_fss_out, &read_fds); 
        int max_fd = (fd_fss_out > STDIN_FILENO) ? fd_fss_out : STDIN_FILENO;//Select biggest fd value for select syscall, no matter what fd open gives to fss_out

        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);//select call NON-BLOCKINGLY
        if(ready == -1) {
            perror("select error");
            exit(1);
        }

        if(FD_ISSET(fd_fss_out, &read_fds)) {
            ssize_t bytes_read = read(fd_fss_out, output_buffer, sizeof(output_buffer) - 1);
            if(bytes_read > 0) {
                output_buffer[bytes_read] = '\0';
                printf("%s", output_buffer);  
                write(fd_console_logfile, output_buffer, bytes_read);
                fflush(stdout);

            } else if(bytes_read == 0) {
                break;
            }
        }

        if(FD_ISSET(STDIN_FILENO, &read_fds)) {
            if(fgets(input, sizeof(input), stdin) == NULL) {
                printf("EOF or input error. Exiting...\n");
                break;
            }

            if(extract_info_from_input(input, fd_console_logfile) == -1) {
                continue;
            }

            size_t len = strlen(input);
            if (len > 0 && input[len - 1] == '\n') {
                input[len - 1] = '\0';
            }

            if (strlen(input) == 0) continue;

            if (write(fd_fss_in, input, strlen(input)) == -1) {
                perror("write to fss_in failed");
                break;
            }
        }
    }

    close(fd_fss_in);
    close(fd_fss_out);
    close(fd_console_logfile);
    unlink("fss_out");
    return 0;
}