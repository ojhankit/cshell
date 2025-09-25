#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>   // for O_WRONLY, O_CREAT, O_TRUNC, O_APPEND

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
    printf("\n");
    fflush(stdout);
}

int main(int argc, char const *argv[]) {
    signal(SIGINT, handler_function); // Ctrl+C handler

    while (1) {
        // Print current working directory as prompt
        char cwd[1024];
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

        // Fork child process
        pid_t pid = fork();
        if (pid == 0) {
            // Child process

            // Check for output redirection
            int output_redirect = 0, append = 0, input_redirect = 0;
            char *outfile = NULL;
            char *infile = NULL;
            for (int i = 0; args[i] != NULL; i++) {
                if (strcmp(args[i], ">") == 0) {
                    output_redirect = 1;
                    append = 0;
                    outfile = args[i + 1];
                    args[i] = NULL;
                    break;
                } else if (strcmp(args[i], ">>") == 0) {
                    output_redirect = 1;
                    append = 1;
                    outfile = args[i + 1];
                    args[i] = NULL;
                    break;
                }
                else if (strcmp(args[i], "<") == 0){
                    input_redirect = 1;
                    infile = args[i+1];
                    args[i] = NULL;
                    break;
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
