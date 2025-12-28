#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_LINES 1000

typedef struct {
    char text[1024];    
    int is_read;        
    int upper_done;     
    int replace_done;  
    int is_written;    
    pthread_mutex_t line_lock; 
} LineEntry;

LineEntry queue[MAX_LINES];
char *filename = NULL;
int total_lines = 0;          
int next_read_index = 0;      
pthread_mutex_t read_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t write_file_mutex = PTHREAD_MUTEX_INITIALIZER; 

void* read_thread_func(void* arg) {
    int id = *(int*)arg;
    while (1) {
        int line_to_process;
        pthread_mutex_lock(&read_mutex);
        if (next_read_index >= total_lines) {
            pthread_mutex_unlock(&read_mutex);
            break;
        }
        line_to_process = next_read_index++;
        pthread_mutex_unlock(&read_mutex);

        FILE *file = fopen(filename, "r");
        if (!file) break;

        char buffer[1024];
        int current = 0;
        int found = 0;
        while (fgets(buffer, sizeof(buffer), file)) {
            if (current == line_to_process) {
                buffer[strcspn(buffer, "\n")] = 0; 
                strcpy(queue[line_to_process].text, buffer);
                queue[line_to_process].is_read = 1;
                found = 1;
                printf("Read_%d\n", id);
                printf("Read_%d read the line %d which is \"%s\"\n", id, line_to_process + 1, buffer);
                break;
            }
            current++;
        }
        fclose(file);
        if (!found) break;
    }
    return NULL;
}

void* upper_thread_func(void* arg) {
    int id = *(int*)arg;
    while (1) {
        int all_done = 1;
        int work_done = 0;
        for (int i = 0; i < total_lines; i++) {
            if (!queue[i].is_written) all_done = 0;
            if (queue[i].is_read && !queue[i].upper_done) {
                if (pthread_mutex_trylock(&queue[i].line_lock) == 0) {
                    char old[1024];
                    strcpy(old, queue[i].text);
                    for (int j = 0; queue[i].text[j]; j++) queue[i].text[j] = toupper(queue[i].text[j]);
                    queue[i].upper_done = 1;
                    printf("Upper_%d\n", id);
                    printf("Upper_%d read index %d and converted \"%s\" to \"%s\"\n", id, i + 1, old, queue[i].text);
                    pthread_mutex_unlock(&queue[i].line_lock);
                    work_done = 1;
                }
            }
        }
        if (all_done && next_read_index >= total_lines && !work_done) break;
        usleep(5000); 
    }
    return NULL;
}

void* replace_thread_func(void* arg) {
    int id = *(int*)arg;
    while (1) {
        int all_done = 1;
        int work_done = 0;
        for (int i = 0; i < total_lines; i++) {
            if (!queue[i].is_written) all_done = 0;
            if (queue[i].is_read && !queue[i].replace_done) {
                if (pthread_mutex_trylock(&queue[i].line_lock) == 0) {
                    char old[1024];
                    strcpy(old, queue[i].text);
                    for (int j = 0; queue[i].text[j]; j++) if (queue[i].text[j] == ' ') queue[i].text[j] = '_';
                    queue[i].replace_done = 1;
                    printf("Replace_%d\n", id);
                    printf("Replace_%d read index %d and converted \"%s\" to \"%s\"\n", id, i + 1, old, queue[i].text);
                    pthread_mutex_unlock(&queue[i].line_lock);
                    work_done = 1;
                }
            }
        }
        if (all_done && next_read_index >= total_lines && !work_done) break;
        usleep(5000);
    }
    return NULL;
}

void* writer_thread_func(void* arg) {
    int id = *(int*)arg;
    while (1) {
        int all_done = 1;
        int work_done = 0;
        for (int i = 0; i < total_lines; i++) {
            if (!queue[i].is_written) {
                all_done = 0;
                if (queue[i].upper_done && queue[i].replace_done) {
                    pthread_mutex_lock(&write_file_mutex);
                    if (!queue[i].is_written) {
                        printf("Writer_%d\n", id);
                        printf("Writer_%d write line %d back which is \"%s\"\n", id, i + 1, queue[i].text);
                        queue[i].is_written = 1;
                        work_done = 1;
                    }
                    pthread_mutex_unlock(&write_file_mutex);
                }
            }
        }
        if (all_done && next_read_index >= total_lines && !work_done) break;
        usleep(5000);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int n_read = 0, n_upper = 0, n_replace = 0, n_write = 0;
    int opt;

    while ((opt = getopt(argc, argv, "d:n:")) != -1) {
        switch (opt) {
            case 'd': filename = optarg; break;
            case 'n': 
                if (optind - 1 < argc) n_read = atoi(argv[optind-1]);
                if (optind < argc) n_upper = atoi(argv[optind]);
                if (optind + 1 < argc) n_replace = atoi(argv[optind+1]);
                if (optind + 2 < argc) n_write = atoi(argv[optind+2]);
                break;
        }
    }

    if (!filename) return 1;
    FILE *fp = fopen(filename, "r");
    if (!fp) return 1;
    char temp[1024];
    while (fgets(temp, sizeof(temp), fp)) total_lines++;
    fclose(fp);

    for(int i = 0; i < MAX_LINES; i++) {
        pthread_mutex_init(&queue[i].line_lock, NULL);
        queue[i].is_read = queue[i].upper_done = queue[i].replace_done = queue[i].is_written = 0;
    }

    pthread_t r_t[n_read], u_t[n_upper], rep_t[n_replace], w_t[n_write];
    int r_ids[n_read], u_ids[n_upper], rep_ids[n_replace], w_ids[n_write];

    for(int i=0; i<n_read; i++) { r_ids[i]=i+1; pthread_create(&r_t[i], NULL, read_thread_func, &r_ids[i]); }
    for(int i=0; i<n_upper; i++) { u_ids[i]=i+1; pthread_create(&u_t[i], NULL, upper_thread_func, &u_ids[i]); }
    for(int i=0; i<n_replace; i++) { rep_ids[i]=i+1; pthread_create(&rep_t[i], NULL, replace_thread_func, &rep_ids[i]); }
    for(int i=0; i<n_write; i++) { w_ids[i]=i+1; pthread_create(&w_t[i], NULL, writer_thread_func, &w_ids[i]); }

    for(int i=0; i<n_read; i++) pthread_join(r_t[i], NULL);
    for(int i=0; i<n_upper; i++) pthread_join(u_t[i], NULL);
    for(int i=0; i<n_replace; i++) pthread_join(rep_t[i], NULL);
    for(int i=0; i<n_write; i++) pthread_join(w_t[i], NULL);

    return 0;
}