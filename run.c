#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>   // for O_WRONLY, O_CREAT, O_TRUNC, O_APPEND

char cwd[1024];
// Function to parse input into args array
void parse_input(char *input, char **args) {
    int i = 0;
    char *token = strtok(input, " ");
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
}

// Signal handler for Ctrl+C
void handler_function(int sig) {
    //printf("\n");
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("\n%s> ", cwd);
    }else {
        perror("getcwd() error");
        printf("\nmyshell> ");
    }
    fflush(stdout);
}

int main(int argc, char const *argv[]) {
    signal(SIGINT, handler_function); // Ctrl+C handler

    while (1) {
        // Print current working directory as prompt
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s> ", cwd);
        } else {
            perror("getcwd() error");
            printf("myshell> ");
        }
        fflush(stdout);

        // Read input
        char ch[1024];
        if (fgets(ch, sizeof(ch), stdin) == NULL) {
            printf("\n");
            break;
        }
        ch[strcspn(ch, "\n")] = 0;

        if (strcmp(ch, "exit") == 0) {
            printf("exit\n");
            break;
        }

        char *args[64];
        parse_input(ch, args);

        // Built-in cd command
        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                char *home = getenv("HOME");
                if (home != NULL && chdir(home) != 0) {
                    perror("cd failed");
                }
            } else {
                if (chdir(args[1]) != 0) {
                    perror("cd failed");
                }
            }
            continue; // skip fork for cd
        }
        // pipe cmd handling
        int has_pipe = 0;
        int pipe_index = -1;
        for(int i=0;args[i] != NULL; i++){
            if(strcmp(args[i],"|") == 0){
                has_pipe = 1;
                pipe_index = i;
                break;
            }
        }

        if(has_pipe){
            args[pipe_index] = NULL; // terminate first cmd
            char *left_args[64];
            char *right_args[64];

            // left cmd
            for(int i=0;i<pipe_index;i++){
                left_args[i] = args[i];
            }
            left_args[pipe_index] = NULL;
            // right cmd
            int j=0;
            for(int i=pipe_index+1;args[i]!=NULL;i++){
                right_args[j++] = args[i];
            }
            right_args[j] = NULL;

            int fd[2];
            if (pipe(fd) == -1){
                perror("pipe failed");
                continue;
            }

            pid_t pid1 = fork();
            if(pid1 == 0){
                // left cmd
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
                execvp(left_args[0],left_args);
                perror("left exec failed");
                exit(1);
            }

            pid_t pid2 = fork();
            if(pid2 == 0){
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]);
                close(fd[1]);
                execvp(right_args[0], right_args);
                perror("exec right failed");
                exit(1);
            }

            close(fd[0]);
            close(fd[1]);
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
        }
        // Fork child process
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            // Check for output redirection
            int output_redirect = 0, append = 0, input_redirect = 0;
            char *outfile = NULL;
            char *infile = NULL;

            for (int i = 0; args[i] != NULL; i++) {
                if (strcmp(args[i], ">") == 0) {
                    output_redirect = 1;
                    append = 0;
                    if(args[i+1]==NULL){
                        perror("Syntax error");
                        exit(1);
                    }
                    outfile = args[i + 1];
                    args[i] = NULL;
                    args[i+1] = NULL;
                    i++;
                    //break;
                } else if (strcmp(args[i], ">>") == 0) {
                    output_redirect = 1;
                    append = 1;
                    if(args[i+1] == NULL){
                        perror("Syntax error");
                        exit(1);
                    }
                    outfile = args[i + 1];
                    args[i] = NULL;
                    args[i+1] = NULL;
                    i++;
                    //break;
                }
                else if (strcmp(args[i], "<") == 0){
                    input_redirect = 1;
                    if(args[i+1] == NULL){
                        perror("Syntax error");
                        exit(1);
                    }
                    infile = args[i+1];
                    args[i] = NULL;
                    args[i+1] = NULL;
                    i++;
                    //break;
                }
            }

            if (output_redirect && outfile != NULL) {
                int fd;
                if (append)
                    fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
                else
                    fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("open failed");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO); // redirect stdout
                close(fd);
            }

            if(input_redirect && infile != NULL){
                int fd = open(infile, O_RDONLY);
                if(fd<0){
                    perror("open failed");
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            execvp(args[0], args);
            perror("exec failed");
            exit(1);

        } else if (pid > 0) {
            // Parent process waits
            waitpid(pid, NULL, 0);
        } else {
            perror("Fork failed");
        }
    }

    return 0;
}
