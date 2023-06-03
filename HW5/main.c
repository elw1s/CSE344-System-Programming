#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>


/* The values below are used for statistics */
int number_of_files = 0;
ssize_t total_bytes_copied = 0;
int total_regular_files = 0;
int total_directories = 0;

typedef struct Task{
    int source_fd;
    int dest_fd;
    char * source_filename;
    char * dest_filename;
}Task;

typedef struct {
    char* source_dir;
    char* dest_dir;
    int isRoot;
} DirectoryPathnames;

Task* taskQueue;
int taskCount = 0;
int done=0;
pthread_mutex_t mutexQueue;
pthread_mutex_t mutexSTDOUT;
pthread_cond_t condQueue;
sem_t semEmpty;
sem_t semFull;
pthread_t* consumers;
pthread_t producer;
int number_of_consumers;
struct timeval start_time, end_time;
long long start_usec, end_usec;
double elapsed_time;


void sigIntHandler(int signum) {
    pthread_mutex_lock(&mutexQueue);
    done = 1;
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_broadcast(&condQueue);
}

void* producerFunction(void* args){
    DirectoryPathnames* directories = (DirectoryPathnames*)args;
    const char* source_dir = directories->source_dir;
    const char* dest_dir = directories->dest_dir;
    DIR* SOURCE_DIR;
    DIR* DEST_DIR;
    struct dirent* entry;
    struct stat fileStat;
    SOURCE_DIR = opendir(source_dir);
    DEST_DIR = opendir(dest_dir);
    if(DEST_DIR == NULL){
        mkdir(dest_dir,0700);
        DEST_DIR = opendir(dest_dir);
    }
    if (SOURCE_DIR == NULL || DEST_DIR == NULL) {
        pthread_mutex_lock(&mutexSTDOUT);
        printf("Unable to open Source/Destination directory\n");
        pthread_mutex_unlock(&mutexSTDOUT);
        //Set done flag so consumer threads also terminates...
        pthread_mutex_lock(&mutexQueue);
        done = 1; // Set done flag
        pthread_mutex_unlock(&mutexQueue);
        pthread_cond_broadcast(&condQueue);
        closedir(SOURCE_DIR);
        closedir(DEST_DIR);
        return NULL;
    }
    char source_filepath[512];
    char dest_filepath[512];
    while ((entry = readdir(SOURCE_DIR)) != NULL){
        pthread_mutex_lock(&mutexQueue);
        if(done == 1){
            pthread_mutex_unlock(&mutexQueue);
            closedir(SOURCE_DIR);
            closedir(DEST_DIR);
            return NULL;
        }
        pthread_mutex_unlock(&mutexQueue);
        // Get the file/directory information
        snprintf(source_filepath, sizeof(source_filepath), "%s/%s", source_dir, entry->d_name);
        snprintf(dest_filepath, sizeof(dest_filepath), "%s/%s", dest_dir, entry->d_name);
        if (stat(source_filepath, &fileStat)) {
            perror("Unable to get file/directory information");
            continue;
        }

        // Check if it is a regular file or directory
        if (S_ISREG(fileStat.st_mode)) {
            int source_fd, dest_fd;

            source_fd = open(source_filepath,O_RDONLY,0666);
            dest_fd = open(dest_filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);

            /* If there is an error at opening file, then print it. */
            if(source_fd == -1 || dest_fd == -1){
                if(errno == EMFILE){
                    perror("Exceeded the per-process limit on open file descriptors.\n");
                }
                else{
                    pthread_mutex_lock(&mutexSTDOUT);
                    printf("Error at opening file\n");
                    pthread_mutex_unlock(&mutexSTDOUT);
                }
                pthread_mutex_lock(&mutexQueue);
                done = 1; // Set done flag
                pthread_mutex_unlock(&mutexQueue);
                pthread_cond_broadcast(&condQueue);
                closedir(SOURCE_DIR);
                closedir(DEST_DIR);
                return NULL; 
            }
            Task task = {
            .source_fd = source_fd,
            .dest_fd = dest_fd
            };
            task.source_filename = (char*)malloc(strlen(source_filepath)+1);
            task.dest_filename = (char*)malloc(strlen(dest_filepath)+1);
            strcpy(task.source_filename, source_filepath);
            strcpy(task.dest_filename, dest_filepath);
            sem_wait(&semEmpty);
            pthread_mutex_lock(&mutexQueue);
            taskQueue[taskCount] = task;
            taskCount++;
            pthread_mutex_unlock(&mutexQueue);
            sem_post(&semFull);
            pthread_cond_signal(&condQueue);
            total_regular_files++;
        } else if (S_ISDIR(fileStat.st_mode) && strcmp(entry->d_name,"..") != 0 && strcmp(entry->d_name,".") != 0) {
            DirectoryPathnames directories;
            directories.source_dir = (char*)malloc(strlen(source_filepath) + 1);
            directories.dest_dir = (char*)malloc(strlen(dest_filepath) + 1);
            directories.isRoot = 0;
            strcpy(directories.source_dir,source_filepath);
            strcpy(directories.dest_dir,dest_filepath);
            producerFunction((void*)&directories);
            free(directories.source_dir);
            free(directories.dest_dir);
            total_directories ++;
        }
    }

    if(directories->isRoot){
        pthread_mutex_lock(&mutexQueue);
        done = 1; // Set done flag
        pthread_mutex_unlock(&mutexQueue);
        pthread_cond_broadcast(&condQueue); 
    }
    closedir(SOURCE_DIR);
    closedir(DEST_DIR);
    return NULL;
}

void* consumerFunction(void* args){
    int isDone = 0;
    while(1){
        pthread_mutex_lock(&mutexQueue);
        while(taskCount == 0 && !done){
            pthread_cond_wait(&condQueue,&mutexQueue);
        }
        isDone = done;
        if(taskCount > 0){
            sem_wait(&semFull);
            Task consumer_task = taskQueue[0];
            int i;
            for (i = 0; i < taskCount - 1; i++) {
                taskQueue[i] = taskQueue[i + 1];
            }
            taskCount --;
            sem_post(&semEmpty);
            pthread_mutex_unlock(&mutexQueue);
            //PROCESS TASK
            char buffer[1024];
            ssize_t bytesRead;
            ssize_t file_bytes = 0;

            while ((bytesRead = read(consumer_task.source_fd, buffer, sizeof(buffer))) > 0) {
                ssize_t bytesWritten = write(consumer_task.dest_fd, buffer, bytesRead);
                pthread_mutex_lock(&mutexQueue);
                total_bytes_copied += bytesWritten;
                pthread_mutex_unlock(&mutexQueue);
                file_bytes += bytesWritten;
                if (bytesWritten == -1) {
                    perror("Error writing to destination file");
                    return NULL;
                }
            }

            if (bytesRead == -1) {
                perror("Error reading from source file");
                return NULL;
            }
            pthread_mutex_lock(&mutexSTDOUT); 
            printf("%zd bytes are copied from %s to %s\n",file_bytes,consumer_task.source_filename,consumer_task.dest_filename);
            pthread_mutex_unlock(&mutexSTDOUT);
            pthread_mutex_lock(&mutexQueue);
            number_of_files++;
            pthread_mutex_unlock(&mutexQueue);
            free(consumer_task.source_filename);
            free(consumer_task.dest_filename);
            close(consumer_task.source_fd);
            close(consumer_task.dest_fd);
        }

        if(isDone){
            pthread_mutex_unlock(&mutexQueue);
            return NULL;
        }
    }

    return NULL;
}

int main(int argc, char * argv[]){

    int i;
    int buffer_size;
    if(argc != 5){
        printf("There should be 4 parameters in the following format: bufferSize numberOfConsumers SourceDir DestDir"); 
        exit(1);   
    }


    buffer_size = atoi(argv[1]);
    number_of_consumers = atoi(argv[2]);
    DirectoryPathnames directories;
    directories.source_dir = (char*)malloc(strlen(argv[3]) + 1);
    directories.dest_dir = (char*)malloc(strlen(argv[4]) + 1);
    strcpy(directories.source_dir,argv[3]);
    strcpy(directories.dest_dir,argv[4]);
    directories.isRoot = 1;

    struct sigaction sa_int;
    sa_int.sa_handler = sigIntHandler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    consumers = (pthread_t*)malloc(number_of_consumers * sizeof(pthread_t));
    taskQueue = (Task*)malloc(buffer_size*sizeof(Task));

    pthread_mutex_init(&mutexQueue,NULL);
    pthread_mutex_init(&mutexSTDOUT,NULL);
    pthread_cond_init(&condQueue,NULL);
    sem_init(&semEmpty, 0, buffer_size);
    sem_init(&semFull, 0, 0);

    gettimeofday(&start_time, NULL);
    start_usec = start_time.tv_sec * 1000000LL + start_time.tv_usec;
    /* Creating Threads */
    if(pthread_create(&producer,NULL,&producerFunction,(void*)&directories)!=0){
        perror("Failed to create producer thread\n");
    }

    for(i = 0; i < number_of_consumers; i++){
        if(pthread_create(&consumers[i],NULL, &consumerFunction, NULL) != 0){
            perror("Failed to create consumer thread\n");
        }
    }
    /* Joining Threads */
    if(pthread_join(producer,NULL)){
        perror("Failed to join producer thread\n");
    }
    else{
        printf("Joined to producer thread\n");
    }
    for(i = 0; i < number_of_consumers; i++){
        if(pthread_join(consumers[i],NULL) != 0){
            perror("Failed to join consumer thread\n");
        }
        else{
            printf("Joined to consumers[%d] thread\n",i);
        }
    }
    gettimeofday(&end_time, NULL);
    end_usec = end_time.tv_sec * 1000000LL + end_time.tv_usec;
    elapsed_time = (end_usec - start_usec) / 1000000.0;
    /* STATISTICS */
    printf("Number of files copied:%d\n",number_of_files);
    printf("Total time for copying:%.6f seconds\n",elapsed_time);
    printf("Total bytes copied:%zd\n",total_bytes_copied);
    printf("Total number of regular files copied:%d\n",total_regular_files);
    printf("Total number of directories copied:%d\n",total_directories);

    pthread_mutex_destroy(&mutexQueue);
    pthread_mutex_destroy(&mutexSTDOUT);
    pthread_cond_destroy(&condQueue);
    sem_destroy(&semEmpty);
    sem_destroy(&semFull);
    free(consumers);
    free(directories.source_dir);
    free(directories.dest_dir);
    free(taskQueue);

    return 0;
}