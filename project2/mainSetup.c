#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_LINE 128 
#define MAX_ARGS 32  
#define MAX_BG_PROCS 50

// stores the foreground process pid
static pid_t foreground_pid = 0;

// stores background process pids
pid_t bg_processes[MAX_BG_PROCS];
int bg_proc_count = 0;

typedef struct {
    char alias[50];
    char command[128];
} Alias;

Alias alias_list[50];
int alias_count = 0;

// add a background process
void add_bg_process(pid_t pid) {
    if (bg_proc_count < MAX_BG_PROCS) {
        bg_processes[bg_proc_count++] = pid;
    }
}

// remove a background process
void remove_bg_process(pid_t pid) {
    for (int i = 0; i < bg_proc_count; i++) {
        if (bg_processes[i] == pid) {

            for (int j = i; j < bg_proc_count - 1; j++) {
                bg_processes[j] = bg_processes[j + 1];
            }

            bg_proc_count--;
            return;
        }
    }
}

// check if a pid is background process
int is_bg_process(pid_t pid) {
    for (int i = 0; i < bg_proc_count; i++) {
        if (bg_processes[i] == pid) return 1;
    }
    return 0;
}

// remove quotes from a string
void clean_quotes(char *str) {
    int len = strlen(str);
    if (len >= 2 && str[0] == '"' && str[len-1] == '"') {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

// add a new alias
void add_alias(char *command, char *name) {
    clean_quotes(command);

    for (int i = 0; i < alias_count; i++) {
        if (strcmp(alias_list[i].alias, name) == 0) {
            strcpy(alias_list[i].command, command);
            return;
        }
    }

    strcpy(alias_list[alias_count].alias, name);
    strcpy(alias_list[alias_count].command, command);
    alias_count++;
}

// print aliases
void list_aliases() {
    for (int i = 0; i < alias_count; i++) {
        printf("%s \"%s\"\n", alias_list[i].alias, alias_list[i].command);
    }
}

// remove alias
void remove_alias(char *name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(alias_list[i].alias, name) == 0) {

            for (int j = i; j < alias_count - 1; j++) {
                alias_list[j] = alias_list[j+1];
            }

            alias_count--;
            return;
        }
    }
    printf("unalias: alias not found: %s\n", name);
}

// handle Ctrl+Z
void handle_sig_tstp(int sig) {
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGKILL);
        printf("\n[Process %d terminated by ^Z]\n", foreground_pid);
        foreground_pid = 0;
    } else {
        printf("\nmyshell: ");
        fflush(stdout);
    }
}

// search command in PATH
char *find_in_path(char *command) {
    if (command == NULL) return NULL;

    // if command already has /
    if (strchr(command, '/') != NULL) return strdup(command);

    char *path_env = getenv("PATH");
    if (path_env == NULL) return NULL;

    char *path_copy = strdup(path_env);
    char *dir = strtok(path_copy, ":");
    static char full_path[MAX_LINE];

    while (dir != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);

        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return full_path;
        }

        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return NULL;
}

// parse user input into args[]
void setup(char inputBuffer[], char *args[], int *background)
{
    int length, i, start, ct;
    
    ct = 0;
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE);

    // Ctrl+D
    if (length == 0) exit(0);

    if ((length < 0) && (errno != EINTR)) {
        perror("error reading the command");
        exit(-1);
    }

    inputBuffer[length] = '\0';
    start = -1;
    int in_quote = 0;

    for (i = 0; i < length; i++) {

        // handle quotes
        if (inputBuffer[i] == '"') {
            in_quote = !in_quote;
            if (start == -1) start = i;
        }

        // handle &
        else if (inputBuffer[i] == '&' && !in_quote) {
            *background = 1;
            inputBuffer[i] = '\0';
        }

        // handle space
        else if (isspace(inputBuffer[i]) && !in_quote) {
            if (start != -1) {
                args[ct++] = &inputBuffer[start];
            }
            inputBuffer[i] = '\0';
            start = -1;
        }

        else {
            if (start == -1) start = i;
        }
    }

    if (start != -1) {
        args[ct++] = &inputBuffer[start];
    }

    args[ct] = NULL;
}

// handle redirection
int handle_redirection(char *args[]) {
    int i = 0;
    int fd;

    while (args[i] != NULL) {

        int mode = 0;

        if (strcmp(args[i], ">") == 0) mode = 1;
        else if (strcmp(args[i], ">>") == 0) mode = 2;
        else if (strcmp(args[i], "<") == 0) mode = 3;
        else if (strcmp(args[i], "2>") == 0) mode = 4;

        if (mode != 0) {

            if (args[i+1] == NULL) {
                fprintf(stderr, "myshell: syntax error\n");
                return -1;
            }
            
            char *filename = args[i+1];

            // open file
            if (mode == 1)
                fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            else if (mode == 2)
                fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
            else if (mode == 3)
                fd = open(filename, O_RDONLY);
            else
                fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd < 0) { perror("open"); return -1; }

            // redirect file descriptors
            if (mode == 3) dup2(fd, STDIN_FILENO);
            else if (mode == 4) dup2(fd, STDERR_FILENO);
            else dup2(fd, STDOUT_FILENO);

            close(fd);

            // remove redirection tokens
            int j = i;
            while (args[j+2] != NULL) {
                args[j] = args[j+2];
                j++;
            }

            args[j] = NULL;
            args[j+1] = NULL;

            continue;
        }

        i++;
    }

    return 0;
}

int main(void)
{
    char inputBuffer[MAX_LINE];
    int background;
    char *args[MAX_ARGS];

    // set Ctrl+Z handler
    signal(SIGTSTP, handle_sig_tstp);

    while (1) {

        background = 0;

        // clean finished background processes
        int status;
        pid_t z_pid;

        while ((z_pid = waitpid(-1, &status, WNOHANG)) > 0) {
            remove_bg_process(z_pid);
        }

        printf("myshell: ");
        fflush(stdout);

        setup(inputBuffer, args, &background);

        if (args[0] == NULL) continue;

        // check if input is alias name
        int alias_found_idx = -1;

        for (int i = 0; i < alias_count; i++) {
            if (strcmp(args[0], alias_list[i].alias) == 0) {
                alias_found_idx = i;
                break;
            }
        }

        // replace alias with real command
        if (alias_found_idx != -1) {

            char temp_cmd[128];
            strcpy(temp_cmd, alias_list[alias_found_idx].command);

            char *remaining_args[MAX_ARGS];
            int r_idx = 0;

            for (int j = 1; args[j] != NULL; j++) {
                remaining_args[r_idx++] = args[j];
            }
            remaining_args[r_idx] = NULL;

            int k = 0;
            char *token = strtok(temp_cmd, " \t");

            while(token != NULL && k < MAX_ARGS - 1) {
                args[k++] = token;
                token = strtok(NULL, " \t");
            }

            for(int j=0; j<r_idx && k < MAX_ARGS -1; j++) {
                args[k++] = remaining_args[j];
            }

            args[k] = NULL;
        }

        // handle exit
        if (strcmp(args[0], "exit") == 0) {

            if (bg_proc_count > 0) {
                fprintf(stderr, "Warning: background processes still running.\n");
                continue;
            }

            exit(0);
        }

        // handle fg
        if (strcmp(args[0], "fg") == 0) {

            if (args[1] != NULL && args[1][0] == '%') {

                pid_t target_pid = atoi(&args[1][1]);

                if (is_bg_process(target_pid)) {

                    printf("myshell: Process %d moved to foreground\n", target_pid);

                    remove_bg_process(target_pid);

                    foreground_pid = target_pid;
                    waitpid(target_pid, &status, 0);
                    foreground_pid = 0;

                } else {
                    fprintf(stderr, "fg: no such background job\n");
                }

            } else {
                fprintf(stderr, "Usage: fg %%num\n");
            }

            continue;
        }

        // handle alias
        if (strcmp(args[0], "alias") == 0) {

            if (args[1] == NULL) {
            }
            else if (strcmp(args[1], "-l") == 0) {
                list_aliases();
            }
            else {
                if (args[2] != NULL) add_alias(args[1], args[2]);
                else fprintf(stderr, "Usage: alias \"command\" name\n");
            }

            continue;
        }

        // handle unalias
        if (strcmp(args[0], "unalias") == 0) {
            if (args[1]) remove_alias(args[1]);
            else fprintf(stderr, "Usage: unalias name\n");
            continue;
        }

        // create a child for external commands
        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
        }

        // child process
        else if (pid == 0) {

            if (handle_redirection(args) < 0) {
                exit(1);
            }

            char *command_path = find_in_path(args[0]);
            if (command_path == NULL) {
                fprintf(stderr, "myshell: command not found: %s\n", args[0]);
                exit(1);
            }

            execv(command_path, args);
            perror("execv failed");
            exit(1);
        }

        // parent process
        else {

            // run in foreground
            if (!background) {
                foreground_pid = pid;
                waitpid(pid, NULL, 0);
                foreground_pid = 0;
            }

            // run in background
            else {
                printf("[Process running in background: PID %d]\n", pid);
                add_bg_process(pid);
            }
        }
    }

    return 0;
}
