#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_LINES 1000

//Structure to hold line information and synchronization status.

typedef struct {
    char text[1024];            // Content of the line
    long offset;                // Byte offset in the file (for direct writing)
    int is_read;                // Flag: Has the line been read?
    int upper_done;             // Flag: Is Upper operation finished?
    int replace_done;           // Flag: Is Replace operation finished?
    int is_written;             // Flag: Has it been written back to file?
    pthread_mutex_t line_lock;  // Mutex for row-level synchronization
} LineEntry;

// Global Shared Variables
LineEntry queue[MAX_LINES];
char *filename = NULL;
int total_lines = 0;          
int next_read_index = 0;      

// Global Mutexes for critical sections
pthread_mutex_t read_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t write_file_mutex = PTHREAD_MUTEX_INITIALIZER; 

//Reads lines from the file one by one using a unique index.

void* read_thread_func(void* arg) {
    int id = *(int*)arg;
    while (1) {
        int line_to_process;
        
        // Critical Section: Get a unique line number
        pthread_mutex_lock(&read_mutex);
        if (next_read_index >= total_lines) {
            pthread_mutex_unlock(&read_mutex);
            break; // No more lines to read
        }
        line_to_process = next_read_index++;
        pthread_mutex_unlock(&read_mutex);

        FILE *file = fopen(filename, "r");
        if (!file) {
            perror("Error opening file in Read Thread");
            pthread_exit(NULL);
        }

        char buffer[1024];
        int current = 0;
        int found = 0;
        long current_pos = 0;

        // Scan file to find the assigned line
        while (1) {
            current_pos = ftell(file); // Save current file pointer position
            if (!fgets(buffer, sizeof(buffer), file)) break;
            
            if (current == line_to_process) {
                // Store data in shared memory
                strcpy(queue[line_to_process].text, buffer);
                queue[line_to_process].offset = current_pos;
                queue[line_to_process].is_read = 1;
                found = 1;

                // Output to console as required
                char print_buf[1024];
                strcpy(print_buf, buffer);
                print_buf[strcspn(print_buf, "\n")] = 0; // Remove newline for printing
                
                printf("Read_%d\n", id);
                printf("Read_%d read the line %d which is \"%s\"\n", id, line_to_process + 1, print_buf);
                break;
            }
            current++;
        }
        fclose(file);
        if (!found) break;
    }
    pthread_exit(NULL);
}

//Converts all characters in the line to Uppercase.

void* upper_thread_func(void* arg) {
    int id = *(int*)arg;
    while (1) {
        int all_done = 1;
        int work_done = 0;

        for (int i = 0; i < total_lines; i++) {
            if (!queue[i].is_written) all_done = 0; 

            // Check if ready for Upper operation
            if (queue[i].is_read && !queue[i].upper_done) {
                // Try to acquire lock. If busy, skip to next line (Non-blocking)
                if (pthread_mutex_trylock(&queue[i].line_lock) == 0) {
                    // Double-check condition after locking
                    if (!queue[i].upper_done) {
                        char old[1024];
                        strcpy(old, queue[i].text);
                        old[strcspn(old, "\n")] = 0;

                        // Convert to Uppercase
                        for (int j = 0; queue[i].text[j]; j++) {
                            queue[i].text[j] = toupper(queue[i].text[j]);
                        }
                        queue[i].upper_done = 1;

                        char new_text[1024];
                        strcpy(new_text, queue[i].text);
                        new_text[strcspn(new_text, "\n")] = 0;

                        printf("Upper_%d\n", id);
                        printf("Upper_%d read index %d and converted \"%s\" to\n\"%s\"\n", id, i + 1, old, new_text);
                        work_done = 1;
                    }
                    pthread_mutex_unlock(&queue[i].line_lock);
                }
            }
        }
        // Exit if all lines are processed and written
        if (all_done && next_read_index >= total_lines && !work_done) break;
        usleep(1000); // Prevent busy waiting
    }
    pthread_exit(NULL);
}

//Replaces all spaces ' ' with underscores '_'.

void* replace_thread_func(void* arg) {
    int id = *(int*)arg;
    while (1) {
        int all_done = 1;
        int work_done = 0;

        for (int i = 0; i < total_lines; i++) {
            if (!queue[i].is_written) all_done = 0;

            if (queue[i].is_read && !queue[i].replace_done) {
                if (pthread_mutex_trylock(&queue[i].line_lock) == 0) {
                    if (!queue[i].replace_done) {
                        char old[1024];
                        strcpy(old, queue[i].text);
                        old[strcspn(old, "\n")] = 0;

                        // Replace spaces with underscores
                        for (int j = 0; queue[i].text[j]; j++) {
                            if (queue[i].text[j] == ' ') queue[i].text[j] = '_';
                        }
                        queue[i].replace_done = 1;

                        char new_text[1024];
                        strcpy(new_text, queue[i].text);
                        new_text[strcspn(new_text, "\n")] = 0;

                        printf("Replace_%d\n", id);
                        printf("Replace_%d read index %d and converted \"%s\" to\n\"%s\"\n", id, i + 1, old, new_text);
                        work_done = 1;
                    }
                    pthread_mutex_unlock(&queue[i].line_lock);
                }
            }
        }
        if (all_done && next_read_index >= total_lines && !work_done) break;
        usleep(1000);
    }
    pthread_exit(NULL);
}

// Writes the modified line back to the file.
 
void* writer_thread_func(void* arg) {
    int id = *(int*)arg;
    while (1) {
        int all_done = 1;
        int work_done = 0;

        for (int i = 0; i < total_lines; i++) {
            if (!queue[i].is_written) {
                all_done = 0;
                // Requirement: Both modifications must be done
                if (queue[i].upper_done && queue[i].replace_done) {
                    
                    // Requirement: Only one writer thread can write to file
                    pthread_mutex_lock(&write_file_mutex);
                    
                    if (!queue[i].is_written) {
                        // Open file in read/update mode ("r+") to overwrite specific line
                        FILE *fp = fopen(filename, "r+");
                        if (fp) {
                            fseek(fp, queue[i].offset, SEEK_SET); // Go to exact line position
                            fputs(queue[i].text, fp); // Overwrite content
                            fclose(fp);
                        } else {
                            perror("Error opening file in Writer Thread");
                        }

                        char clean_text[1024];
                        strcpy(clean_text, queue[i].text);
                        clean_text[strcspn(clean_text, "\n")] = 0;

                        printf("Writer_%d\n", id);
                        printf("Writer_%d write line %d back which is\n\"%s\"\n", id, i + 1, clean_text);
                        
                        queue[i].is_written = 1;
                        work_done = 1;
                    }
                    pthread_mutex_unlock(&write_file_mutex);
                }
            }
        }
        if (all_done && next_read_index >= total_lines && !work_done) break;
        usleep(1000);
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int n_read = 0, n_upper = 0, n_replace = 0, n_write = 0;
    int opt;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "d:n")) != -1) {
        switch (opt) {
            case 'd': 
                filename = optarg; 
                break;
            case 'n': 
                if (optind < argc) n_read = atoi(argv[optind++]);
                if (optind < argc) n_upper = atoi(argv[optind++]);
                if (optind < argc) n_replace = atoi(argv[optind++]);
                if (optind < argc) n_write = atoi(argv[optind++]);
                break;
        }
    }

    // Error Checking: Filename argument
    if (!filename) {
        fprintf(stderr, "Usage: ./project3.out -d <filename> -n <n_read> <n_upper> <n_replace> <n_write>\n");
        return 1;
    }

    // Calculate total lines in file
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening input file");
        return 1;
    }
    char temp[1024];
    while (fgets(temp, sizeof(temp), fp)) total_lines++;
    fclose(fp);

    // Initialize Queue and Mutexes
    for(int i = 0; i < MAX_LINES; i++) {
        if (pthread_mutex_init(&queue[i].line_lock, NULL) != 0) {
            perror("Mutex init failed");
            return 1;
        }
        queue[i].is_read = 0;
        queue[i].upper_done = 0;
        queue[i].replace_done = 0;
        queue[i].is_written = 0;
        queue[i].offset = 0;
    }

    // Allocate thread identifiers
    pthread_t r_t[n_read], u_t[n_upper], rep_t[n_replace], w_t[n_write];
    int r_ids[n_read], u_ids[n_upper], rep_ids[n_replace], w_ids[n_write];
    int err;

    // Create Threads with Error Checking
    for(int i=0; i<n_read; i++) { 
        r_ids[i]=i+1; 
        err = pthread_create(&r_t[i], NULL, read_thread_func, &r_ids[i]); 
        if(err != 0) fprintf(stderr, "Error creating Read thread %d\n", i);
    }
    for(int i=0; i<n_upper; i++) { 
        u_ids[i]=i+1; 
        err = pthread_create(&u_t[i], NULL, upper_thread_func, &u_ids[i]); 
        if(err != 0) fprintf(stderr, "Error creating Upper thread %d\n", i);
    }
    for(int i=0; i<n_replace; i++) { 
        rep_ids[i]=i+1; 
        err = pthread_create(&rep_t[i], NULL, replace_thread_func, &rep_ids[i]); 
        if(err != 0) fprintf(stderr, "Error creating Replace thread %d\n", i);
    }
    for(int i=0; i<n_write; i++) { 
        w_ids[i]=i+1; 
        err = pthread_create(&w_t[i], NULL, writer_thread_func, &w_ids[i]); 
        if(err != 0) fprintf(stderr, "Error creating Writer thread %d\n", i);
    }

    // Wait for all threads to complete
    for(int i=0; i<n_read; i++) pthread_join(r_t[i], NULL);
    for(int i=0; i<n_upper; i++) pthread_join(u_t[i], NULL);
    for(int i=0; i<n_replace; i++) pthread_join(rep_t[i], NULL);
    for(int i=0; i<n_write; i++) pthread_join(w_t[i], NULL);

    // Clean up resources
    for(int i = 0; i < MAX_LINES; i++) {
        pthread_mutex_destroy(&queue[i].line_lock);
    }
    pthread_mutex_destroy(&read_mutex);
    pthread_mutex_destroy(&write_file_mutex);

    return 0;
}