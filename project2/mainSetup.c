#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h> // open() flagleri için gerekli

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */
#define MAX_ARGS 40
#define MAX_BG_PROCS 50

static pid_t foreground_pid = 0;
pid_t bg_processes[MAX_BG_PROCS];
int bg_proc_count = 0;

typedef struct {
    char alias[50];
    char command[200];
} Alias;

Alias alias_list[50];
int alias_count = 0;

void add_bg_process(pid_t pid) {
    if (bg_proc_count < MAX_BG_PROCS) {
        bg_processes[bg_proc_count++] = pid;
    }
}

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

int is_bg_process(pid_t pid) {
    for (int i = 0; i < bg_proc_count; i++) {
        if (bg_processes[i] == pid) return 1;
    }
    return 0;
}

void add_alias(char *command, char *name) {
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
void list_aliases() {
    for (int i = 0; i < alias_count; i++) {
        printf("%s \"%s\"\n", alias_list[i].alias, alias_list[i].command);
    }
}
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
 char *find_in_path(char *command) {
    if (command == NULL) return NULL;

    if (strchr(command, '/') != NULL) {
        return strdup(command);
    }

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
/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */

void setup(char inputBuffer[], char *args[],int *background)
{
    int length, /* # of characters in the command line */
        i=0,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct;     /* index of where to place the next parameter into args[] */
    
    ct = 0;
        
    /* read what the user enters on the command line */
    length = read(STDIN_FILENO,inputBuffer,MAX_LINE);  

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
	exit(-1);           /* terminate with error code of -1 */
    }
    inputBuffer[length] = '\0'; // Null terminate handled simpler
	printf(">>%s<<",inputBuffer);
    
     for (i=0;i<length;i++){ 
        switch (inputBuffer[i]){
        case ' ':
        case '\t' :               
        if(start != -1){
                    args[ct] = &inputBuffer[start];    
            ct++;
        }
                inputBuffer[i] = '\0'; 
        start = -1;
        break;

            case '\n':                 
        if (start != -1){
                    args[ct] = &inputBuffer[start];     
            ct++;
        }
                inputBuffer[i] = '\0';
                args[ct] = NULL; 
        break;
        case '&':
                *background = 1;
                inputBuffer[i] = '\0';
                if (start != -1) {
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                start = -1;
                break;

        default :             
            if (start == -1)
            start = i;   
        break;
    } 
}
     args[ct] = NULL; 

	for (i = 0; i <= ct; i++)
		printf("args %d = %s\n",i,args[i]);
} 

int handle_redirection(char *args[]) {
    int i = 0;
    int fd;

    while (args[i] != NULL) {
        // Output Redirection (>): Truncate
        if (strcmp(args[i], ">") == 0) {
            if (args[i+1] == NULL) {
                fprintf(stderr, "myshell: syntax error near unexpected token `newline'\n");
                return -1;
            }
            fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { perror("open"); return -1; }
            dup2(fd, STDOUT_FILENO); // stdout artık dosyaya gider
            close(fd);
            args[i] = NULL; // execv argüman listesini burada kesiyoruz
            return 0; // Tek seferde genelde tek redirection olur, loop'tan çıkabiliriz veya devam edebiliriz.
        }
        // Output Redirection (>>): Append
        else if (strcmp(args[i], ">>") == 0) {
            if (args[i+1] == NULL) {
                fprintf(stderr, "myshell: syntax error\n");
                return -1;
            }
            fd = open(args[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) { perror("open"); return -1; }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL;
            return 0;
        }
        // Input Redirection (<)
        else if (strcmp(args[i], "<") == 0) {
            if (args[i+1] == NULL) {
                fprintf(stderr, "myshell: syntax error\n");
                return -1;
            }
            fd = open(args[i+1], O_RDONLY);
            if (fd < 0) { perror("open"); return -1; }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL;
            // Input sonrası output redirection da olabilir, o yüzden return etmeyip devam edebiliriz
            // Ancak args dizisini bozduğumuz için indekslemeyi dikkatli yönetmeliyiz.
            // Basitlik adına burada kesiyoruz (genelde komut < dosya > dosya şeklinde parse daha kompleks olur)
        }
        // Error Redirection (2>)
        else if (strcmp(args[i], "2>") == 0) {
            if (args[i+1] == NULL) {
                fprintf(stderr, "myshell: syntax error\n");
                return -1;
            }
            fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { perror("open"); return -1; }
            dup2(fd, STDERR_FILENO);
            close(fd);
            args[i] = NULL;
            return 0;
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
    
    signal(SIGTSTP, handle_sig_tstp);

    while (1) {
        background = 0;
        
        // Zombi süreçleri temizle ve listeyi güncelle
        int status;
        pid_t z_pid;
        while ((z_pid = waitpid(-1, &status, WNOHANG)) > 0) {
            // Eğer process arka plan listesindeyse sil
            if (is_bg_process(z_pid)) {
                remove_bg_process(z_pid);
                // Prompt bozulmasın diye bazen yazdırmazlar ama debug için iyidir:
                // printf("[Done] %d\n", z_pid); 
            }
        }

        printf("myshell: ");
        fflush(stdout); 

        setup(inputBuffer, args, &background);

        if (args[0] == NULL) continue;

        // --- ALIAS CHECK (Recursive yerine basit değiştirme) ---
        for (int i = 0; i < alias_count; i++) {
             if (strcmp(args[0], alias_list[i].alias) == 0) {
                 // Alias bulundu, komutu parse et
                 char temp_cmd[200];
                 strcpy(temp_cmd, alias_list[i].command);
                 
                 // Argümanları yeniden oluştur
                 // Not: Bu basit yaklaşım sadece komutu değiştirir, argüman eklemesini desteklemez.
                 // Örn: alias 'ls -l' list -> 'list' yazınca 'ls -l' çalışır.
                 
                 // Mevcut argümanları kaydırmamız gerekebilir ama basitlik için
                 // args dizisini baştan kuruyoruz.
                 int k = 0;
                 char *token = strtok(temp_cmd, " ");
                 while(token != NULL) {
                     args[k++] = strdup(token); 
                     token = strtok(NULL, " ");
                 }
                 args[k] = NULL;
                 break; 
             }
         }

        /* --- Exit --- */
        if (strcmp(args[0], "exit") == 0) {
            // Arka planda çalışan var mı kontrol et
            if (bg_proc_count > 0) {
                fprintf(stderr, "Warning: There are background processes still running.\n");
                fprintf(stderr, "Please terminate them before exiting.\n");
                continue; 
            }
            exit(0);
        }

        /* --- FG (Foreground) --- */
        if (strcmp(args[0], "fg") == 0) {
            if (args[1] != NULL && args[1][0] == '%') {
                pid_t target_pid = atoi(&args[1][1]);
                
                if (is_bg_process(target_pid)) {
                    printf("Process %d bringing to foreground...\n", target_pid);
                    remove_bg_process(target_pid); // Artık foreground, listeden çıkar
                    foreground_pid = target_pid;
                    waitpid(target_pid, &status, 0);
                    foreground_pid = 0;
                } else {
                    fprintf(stderr, "fg: no such background job: %d\n", target_pid);
                }
            } else {
                fprintf(stderr, "Usage: fg %%num\n");
            }
            continue; 
        }

        /* --- ALIAS / UNALIAS --- */
        if (strcmp(args[0], "alias") == 0) {
            if (args[1] != NULL && strcmp(args[1], "-l") == 0) {
                list_aliases();
            } else if (args[1] != NULL) {
                char command_buffer[200] = {0};
                char name_buffer[50] = {0};

                int arg_idx = 1;
                if (args[1][0] == '"') {
                    strcat(command_buffer, args[1] + 1); 
                    while (args[arg_idx] != NULL && args[arg_idx][strlen(args[arg_idx])-1] != '"') {
                        arg_idx++;
                        if(args[arg_idx]) { strcat(command_buffer, " "); strcat(command_buffer, args[arg_idx]); }
                    }
                    if(args[arg_idx]) command_buffer[strlen(command_buffer)-1] = '\0'; 
                    arg_idx++;
                } else {
                    strcpy(command_buffer, args[1]);
                    arg_idx++;
                }
                
                if (args[arg_idx]) {
                    strcpy(name_buffer, args[arg_idx]);
                    add_alias(command_buffer, name_buffer);
                } else {
                    fprintf(stderr, "Usage: alias \"command\" name\n");
                }
            }
            continue;
        }
        
        if (strcmp(args[0], "unalias") == 0) {
            if (args[1]) remove_alias(args[1]);
            else fprintf(stderr, "Usage: unalias name\n");
            continue;
        }

        /* --- FORK & EXEC --- */
        pid_t pid = fork();

        if (pid < 0) {
            perror("Fork failed");
        }
        else if (pid == 0) {
            /* Child Process */
            
            // 1. Redirection Handle Et
            if (handle_redirection(args) < 0) {
                exit(1);
            }
            
            // 2. Path Bul
            char *command_path = find_in_path(args[0]);
            if (command_path == NULL) {
                fprintf(stderr, "myshell: command not found: %s\n", args[0]);
                exit(1);
            }

            // 3. Exec
            execv(command_path, args);
            perror("execv failed"); // Sadece execv dönerse çalışır
            exit(1);
        }
        else {
            /* Parent Process */
            if (background == 0) {
                foreground_pid = pid;
                waitpid(pid, NULL, 0);
                foreground_pid = 0;
            } else {
                printf("[Process running in background with PID: %d]\n", pid);
                add_bg_process(pid); // Arka plan listesine ekle
            }
        }
    } 
    return 0;
} 

                        /** the steps are:
                        (1) fork a child process using fork()
                        (2) the child process will invoke execv()
						(3) if background == 0, the parent will wait,
                        otherwise it will invoke the setup() function again. */
            

