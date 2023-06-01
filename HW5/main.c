#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h> //Daha sonra sil
#include <errno.h>
#include <fcntl.h>
#include <string.h>


/* The values below are used for statistics */
int number_of_files = 0;
//Types of files tut

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
pthread_cond_t condQueue;
sem_t semEmpty;
sem_t semFull;

/* Done flagini implement ederken dikkat et: Recursive olacağından dolayı while sonrasına koyarsan bu sefer ilk recursive sonunda bitirir. Bunu engelle */
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
    printf("SOURCE_DIR: %s\n",source_dir);
    printf("DEST_DIR: %s\n",dest_dir);
    if(DEST_DIR == NULL){
        mkdir(dest_dir,0700);
        DEST_DIR = opendir(dest_dir);
    }
    if (SOURCE_DIR == NULL || DEST_DIR == NULL) {
        perror("Unable to open Source/Destination directory\n"); 
        //Set done flag so consumer threads also terminates...
        pthread_mutex_lock(&mutexQueue);
        done = 1; // Set done flag
        pthread_mutex_unlock(&mutexQueue);
        pthread_cond_broadcast(&condQueue); 
        return NULL; //Exit mi return mü gelmeli buraya?
    }
    char source_filepath[512];
    char dest_filepath[512];
    while ((entry = readdir(SOURCE_DIR)) != NULL) {
        // Get the file/directory information
        snprintf(source_filepath, sizeof(source_filepath), "%s/%s", source_dir, entry->d_name);
        printf("source_filepath= %s\n",source_filepath);
        snprintf(dest_filepath, sizeof(dest_filepath), "%s/%s", dest_dir, entry->d_name);
        printf("dest_filepath= %s\n",dest_filepath);
        if (stat(source_filepath, &fileStat)) {
            perror("Unable to get file/directory information");
            continue;
        }

        // Check if it is a regular file or directory
        if (S_ISREG(fileStat.st_mode)) {
            //Todo: Burada consumerlar için buffera Task ekle.
            int source_fd, dest_fd;

            source_fd = open(source_filepath,O_RDONLY,0666);
            dest_fd = open(dest_filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);

            /* If there is an error at opening file, then print it. */
            if(source_fd == -1 || dest_fd == -1){
                if(errno == EMFILE){
                    perror("Exceeded the per-process limit on open file descriptors.\n");
                }
                else{
                    perror("Error at opening file\n");
                }
                pthread_mutex_lock(&mutexQueue);
                done = 1; // Set done flag
                pthread_mutex_unlock(&mutexQueue);
                pthread_cond_broadcast(&condQueue);
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
            number_of_files++;
            pthread_mutex_unlock(&mutexQueue);
            sem_post(&semFull);
            pthread_cond_signal(&condQueue);
        } else if (S_ISDIR(fileStat.st_mode) && strcmp(entry->d_name,"..") != 0 && strcmp(entry->d_name,".") != 0) {
            //Todo: Burada recursive olarak tekrar bu methodu çağır. isRoot = 0 olmalı çağrılırken
            printf("There is a directory named %s\n", entry->d_name);
            DirectoryPathnames directories;
            directories.source_dir = (char*)malloc(strlen(source_filepath) + 1);
            directories.dest_dir = (char*)malloc(strlen(dest_filepath) + 1);
            directories.isRoot = 0;
            strcpy(directories.source_dir,source_filepath);
            strcpy(directories.dest_dir,dest_filepath);
            producerFunction((void*)&directories);
        }
    }

    if(directories->isRoot){
        pthread_mutex_lock(&mutexQueue);
        printf("Done is set to 1 in producer\n");
        done = 1; // Set done flag
        pthread_mutex_unlock(&mutexQueue);
        pthread_cond_broadcast(&condQueue); 
    }
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

            while ((bytesRead = read(consumer_task.source_fd, buffer, sizeof(buffer))) > 0) {
                ssize_t bytesWritten = write(consumer_task.dest_fd, buffer, bytesRead);
                if (bytesWritten == -1) {
                    perror("Error writing to destination file");
                    return NULL;
                }
            }

            if (bytesRead == -1) {
                perror("Error reading from source file");
                return NULL;
            } 
            printf("Got TID=%lu SourceFD:%d DestFD:%d SFN=%s DFN=%s\n",pthread_self(),consumer_task.source_fd,consumer_task.dest_fd,consumer_task.source_filename,consumer_task.dest_filename);
            close(consumer_task.source_fd);
            close(consumer_task.dest_fd);
        }

        if(isDone){
            pthread_mutex_unlock(&mutexQueue);
            printf("Terminating from %lu\n",pthread_self());
            return NULL;
        }
    }

    return NULL;
}


/* 

./main 5 3 /home/ardakilic/Desktop/TestFolderHW5 /home/ardakilic/Desktop

 */

int main(int argc, char * argv[]){

    int i;
    int buffer_size, number_of_consumers;
    char * source_dir;
    char * dest_dir;

    if(argc != 5){
        printf("There should be 4 parameters in the following format: bufferSize numberOfConsumers SourceDir DestDir"); 
        exit(1);   
    }

    buffer_size = atoi(argv[1]);
    number_of_consumers = atoi(argv[2]);
    DirectoryPathnames directories;
    directories.source_dir = (char*)malloc(strlen(argv[3]) + 1);
    directories.dest_dir = (char*)malloc(strlen(argv[4]) + 1);
    //source_dir = (char*)malloc(sizeof(argv[3]));
    //dest_dir = (char*)malloc(sizeof(argv[4]));
    printf("CommandLineSource:%s\n",argv[3]);
    printf("CommandLineDestintion:%s\n",argv[4]);
    strcpy(directories.source_dir,argv[3]);
    strcpy(directories.dest_dir,argv[4]);
    printf("directories.source_dir:%s\n",directories.source_dir);
    printf("directories.dest_dir:%s\n",directories.dest_dir);
    directories.isRoot = 1;

    pthread_t producer;
    pthread_t consumers[number_of_consumers];
    taskQueue = (Task*)malloc(buffer_size*sizeof(Task));

    pthread_mutex_init(&mutexQueue,NULL);
    pthread_cond_init(&condQueue,NULL);
    sem_init(&semEmpty, 0, buffer_size);
    sem_init(&semFull, 0, 0);

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

    /* STATISTICS */
    printf("Number of files copied:%d\n",number_of_files);

    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
    sem_destroy(&semEmpty);
    sem_destroy(&semFull);

    return 0;
}