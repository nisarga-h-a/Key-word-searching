#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
// ------------------- Shared Data -------------------
int *shared_total; // shared memory pointer
int completed_threads = 0; // number of threads finished
pthread_mutex_t count_mutex; // protects shared variables
pthread_cond_t done_cond; // signals when all threads
complete
char keyword[100]; // keyword to search
// Structure for each thread’s argument
typedef struct {
char filename[256];
} ThreadArg;
// ------------------- Function to count
keyword in a file -------------------
int count_keyword_in_file(const char
*filename, const char *keyword) {
FILE *fp = fopen(filename, "r");
if (!fp) {
perror("Error opening file");
return 0;
}
char line[512];
int count = 0;
while (fgets(line, sizeof(line), fp)) {
char *pos = line;
while ((pos = strstr(pos, keyword)) != NULL) {
count++;
pos += strlen(keyword);
}
}
fclose(fp);
return count;
}
// ------------------- Thread Function -------------------
void *search_thread(void *arg) {
ThreadArg *targ = (ThreadArg *)arg;
int count = count_keyword_in_file(targ->filename, keyword);
pthread_mutex_lock(&count_mutex);
*shared_total += count; // update shared memory
total
completed_threads++;
printf("%s → %d matches\n", targ->filename, count);
pthread_cond_signal(&done_cond);
pthread_mutex_unlock(&count_mutex);
pthread_exit(NULL);
}
// ------------------- Main Function -------------------
int main(int argc, char *argv[]) {
if (argc != 3) {
printf("Usage: %s <keyword> <directory>\n", argv[0]);
return -1;
}
strcpy(keyword, argv[1]);
char *dirpath = argv[2];
pthread_mutex_init(&count_mutex, NULL);
pthread_cond_init(&done_cond, NULL);
// ----------- Create Shared Memory -----------
int shm_fd = shm_open("/shared_count", O_CREAT | O_RDWR, 0666);
if (shm_fd == -1) {
perror("shm_open failed");
exit(1);
}
ftruncate(shm_fd, sizeof(int)); // allocate memory
shared_total = mmap(0, sizeof(int), PROT_READ | PROT_WRITE,
MAP_SHARED, shm_fd, 0);
if (shared_total == MAP_FAILED) {
perror("mmap failed");
exit(1);
}
*shared_total = 0;
// ----------- Open directory and create threads -----------
DIR *dir = opendir(dirpath);
if (!dir) {
perror("Error opening directory");
return -1;
}
struct dirent *entry;
pthread_t threads[100];
ThreadArg args[100];
int thread_count = 0;
while ((entry = readdir(dir)) != NULL) {
char fullpath[512];
snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
struct stat file_stat;
if (stat(fullpath, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
snprintf(args[thread_count].filename,
sizeof(args[thread_count].filename), "%s", fullpath);
pthread_create(&threads[thread_count], NULL, search_thread,
&args[thread_count]);
thread_count++;
}
}
closedir(dir);
// ----------- Wait for all threads to finish -----------
pthread_mutex_lock(&count_mutex);
while (completed_threads < thread_count) {
pthread_cond_wait(&done_cond, &count_mutex);
}
pthread_mutex_unlock(&count_mutex);
printf("\n Total keyword occurrences (from shared
memory): %d\n", *shared_total);
// ----------- Cleanup -----------
pthread_mutex_destroy(&count_mutex);
pthread_cond_destroy(&done_cond);
munmap(shared_total, sizeof(int));
shm_unlink("/shared_count");
return 0;
}