#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
 #include <errno.h>

#define MAX_FILENAME 1022
#define MAX_MESSAGE 1022
#define BUFFER_SIZE 1024

char* encoder(char com, char dir, const char* msg) {
    char* encodedArray = (char*)malloc(sizeof(char) * 1024);
    strncpy(encodedArray, &com, 1);  // Copy "com" into the first element
    strncpy(encodedArray+1, &dir, 1);  // Copy "dir" into the first element
    strncpy(encodedArray + 2, msg, 1022);  // Copy "msg" into the remaining elements

    return encodedArray;
}
void decoder(const char* encodedArray, char*** parsedCommand) {
    memset((*parsedCommand)[0], 0, 2);
    memset((*parsedCommand)[1], 0, 2);
    memset((*parsedCommand)[2], 0, 1023);

    strncpy((*parsedCommand)[0], encodedArray, 1);  // Copy "com" from the encodedArray
    (*parsedCommand)[0][1] = '\0';  // Set null terminator for parsedCommand[0]
    strncpy((*parsedCommand)[1], encodedArray + 1, 1);  // Copy "dir" from the encodedArray
    (*parsedCommand)[1][1] = '\0';  // Set null terminator for parsedCommand[1]
    strncpy((*parsedCommand)[2], encodedArray + 2, 1022);  // Copy "msg" from the encodedArray
    (*parsedCommand)[2][1022] = '\0';  // Set null terminator for parsedCommand[2]
}
struct FileNode {
    char filename[MAX_FILENAME];
    time_t lastModificationTime;
    int newAddition;
    int isDirectory; 
    struct FileNode* next;
};
struct FileNode* createFileNode(const char* filename, time_t lastModificationTime, int newAddition, int isDirectory) {
    struct FileNode* newNode = (struct FileNode*)malloc(sizeof(struct FileNode));
    strncpy(newNode->filename, filename, MAX_FILENAME);
    newNode->lastModificationTime = lastModificationTime;
    newNode->newAddition = newAddition;
    newNode->isDirectory = isDirectory;
    newNode->next = NULL;
    return newNode;
}
void insertFile(struct FileNode** files, const char* filename, time_t lastModificationTime, int newAddition, int isDirectory) {
    struct FileNode* newNode = createFileNode(filename, lastModificationTime, newAddition, isDirectory);
    if (*files == NULL) {
        *files = newNode;
    } else {
        struct FileNode* current = *files;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }
}
void removeFirstNode(struct FileNode** arr) {
    if (*arr == NULL) {
        return; // No nodes in the list
    }

    struct FileNode* temp = *arr;
    *arr = (*arr)->next;
    free(temp);
}
void printFiles(struct FileNode* files) {
    struct FileNode* current = files;
    while (current != NULL) {
        printf("File: %s | Last Modification Time: %ld\n", current->filename, current->lastModificationTime);
        current = current->next;
    }
}
int countFiles(struct FileNode* files) {
    int count = 0;
    struct FileNode* current = files;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}
void freeFileList(struct FileNode* fileList) {
    int i = 0;
    while (fileList != NULL) {
        struct FileNode* temp = fileList;
        fileList = fileList->next;
        free(temp);
        i++;
    }
}
void traverseDirectory(const char* directoryPath, struct FileNode** fileList, struct FileNode** lastNode) {
    DIR* dir = opendir(directoryPath);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, "clientLog") == 0)
            continue;

        char filepath[MAX_FILENAME];
        snprintf(filepath, sizeof(filepath), "%s/%s", directoryPath, entry->d_name);
        struct stat fileStat;
        if (stat(filepath, &fileStat) == -1) {
            perror("stat");
            continue;
        }

        time_t lastModificationTime = fileStat.st_mtime;
        int isDirectory = S_ISDIR(fileStat.st_mode) ? 1 : 0;
        struct FileNode* newNode = createFileNode(filepath, lastModificationTime, 0, isDirectory);

        if (*fileList == NULL) {
            *fileList = newNode;
            *lastNode = newNode;
        } else {
            (*lastNode)->next = newNode;
            *lastNode = newNode;
        }

        if (S_ISDIR(fileStat.st_mode)) {
            traverseDirectory(filepath, fileList, lastNode);
        }
    }

    closedir(dir);
}
void getCurrentFilesFromDirectory(const char* directoryPath, struct FileNode** fileList) {
    struct FileNode* lastNode = NULL;
    freeFileList(*fileList);
    *fileList = NULL; // Initialize fileList to NULL
    traverseDirectory(directoryPath, fileList, &lastNode);
}
struct FileNode* getNodeListByDirectory(char* directory, struct FileNode* head) {
    struct FileNode* resultHead = NULL;  // Head of the resulting linked list
    struct FileNode* resultTail = NULL;  // Tail of the resulting linked list
    
    struct FileNode* current = head;
    
    // Traverse through the linked list
    while (current != NULL) {
        // Check if the filename matches the given directory
        if (strncmp(directory, current->filename, strlen(directory)) == 0 &&
            current->filename[strlen(directory)] == '/') {
            // Create a new node with the modified filename
            struct FileNode* newNode = (struct FileNode*)malloc(sizeof(struct FileNode));
            strncpy(newNode->filename, current->filename + strlen(directory) + 1, MAX_FILENAME);
            newNode->lastModificationTime = current->lastModificationTime;
            newNode->newAddition = current->newAddition;
            newNode->isDirectory = current->isDirectory;
            newNode->next = NULL;
            
            // If the result list is empty, update the head and tail
            if (resultHead == NULL) {
                resultHead = newNode;
                resultTail = newNode;
            } else {
                // Append the new node to the result list
                resultTail->next = newNode;
                resultTail = newNode;
            }
        }
        
        // Move to the next node
        current = current->next;
    }
    
    // Return the head of the resulting linked list
    return resultHead;
}
void compareFiles(struct FileNode ** updatedArray,const char* rootPath,const char* directoryPath, struct FileNode* array, int* count) {
    DIR* directory = opendir(directoryPath);
    if (directory == NULL) {
        perror("Error opening directory");
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        char filePath[MAX_FILENAME];
        snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);
        
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, "clientLog") == 0){
            continue;
        }
        struct stat fileStat;
        if (stat(filePath, &fileStat) == -1) {
            perror("Error getting file stat");
            continue;
        }

        if (!S_ISDIR(fileStat.st_mode)) {
            // Regular file
            struct FileNode* current = array;
            int found = 0;
            while (current != NULL) {
                if (strcmp(current->filename, entry->d_name) == 0) {
                    found = 1;
                    if (current->lastModificationTime != fileStat.st_mtime) {
                        current->lastModificationTime = fileStat.st_mtime;
                        // Add to updated array
                        insertFile(updatedArray, filePath, current->lastModificationTime, 0, current->isDirectory);
                    }
                    break;
                }
                current = current->next;
            }

            if (!found) {
                // Add to updated array
                insertFile(updatedArray, filePath, fileStat.st_mtime, 1, 0);
            }
        }  else {
            // Directory
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                
                struct FileNode* current = array;
                int found = 0;
                while (current != NULL) {
                    if (strcmp(current->filename, entry->d_name) == 0) {
                        found = 1;
                        break;
                    }
                    current = current->next;
                }

                if (!found) {
                    // Add to updated array
                    insertFile(updatedArray, filePath, fileStat.st_mtime, 1, 1);
                }

                char subDirectoryPath[MAX_FILENAME];
                snprintf(subDirectoryPath, sizeof(subDirectoryPath), "%s/%s", directoryPath, entry->d_name);
                struct FileNode* updatedSubArray = NULL;
                compareFiles(&updatedSubArray,rootPath,subDirectoryPath, getNodeListByDirectory(entry->d_name,array), count);

                // Merge the updated subarray with the main updated array
                current = updatedSubArray;
                while (current != NULL) {
                    insertFile(updatedArray, current->filename, current->lastModificationTime, current->newAddition, current->isDirectory);
                    current = current->next;
                }
                // Free the memory allocated for the subarray
                freeFileList(updatedSubArray);
            } 
        } 
    }

    closedir(directory);

    // Check for removed files in the array
    struct FileNode* current = array;
    if(strcmp(rootPath,directoryPath) == 0){
        while (current != NULL) {
            int filePathSize = strlen(directoryPath) + strlen(current->filename) + 2;
            char filePath[filePathSize];
            snprintf(filePath, filePathSize, "%s/%s", directoryPath, current->filename);
            if (access(filePath, F_OK) == -1) {
                // File doesn't exist, remove from updated array
                insertFile(updatedArray, filePath, -1, -1, current->isDirectory);
            }
            current = current->next;
        }

        int updatedCount = 0;
        current = *updatedArray;
        while (current != NULL) {
            updatedCount++;
            current = current->next;
        }
        *count = updatedCount;
    }
    
    // Count the number of elements in the updated array
    int updatedCount = 0;
    current = *updatedArray;
    while (current != NULL) {
        updatedCount++;
        current = current->next;
    }

    *count = updatedCount;
    free(current);
}
int isFileAvailableInDirectory(const char* directoryPath, const char* filename) {
    char entryPath[2048];
    snprintf(entryPath, sizeof(entryPath), "%s/%s", directoryPath, filename);

    if (access(entryPath, F_OK) == 0) {
        return 1;
    } else {
        return 0;
    }
}
int isDirectoryAvailableInDirectory(const char* directoryPath, const char* directoryName) {
    char entryPath[2048];
    snprintf(entryPath, sizeof(entryPath), "%s/%s", directoryPath, directoryName);

    if (access(entryPath, F_OK) == 0) {
        return 1;
    } else {
        return 0;
    }
}
void modifyFilenames(struct FileNode* head, const char* directoryPath) {
    struct FileNode* current = head;
    char * filename;
    char* relativePath;
    // Traverse the linked list
    while (current != NULL) {
        filename = current->filename;
        if (strstr(filename, directoryPath) == filename) {
            relativePath = filename + strlen(directoryPath);

            // Skip any leading slashes
            while (relativePath[0] == '/')
                relativePath++;
            strcpy(filename, relativePath);
        } else {
            // Prepend the root directory path to the filename
            char modifiedFilename[MAX_FILENAME + strlen(directoryPath)];
            snprintf(modifiedFilename, sizeof(modifiedFilename), "%s/%s", directoryPath, filename);
            strcpy(filename, modifiedFilename);
        }
        // Move to the next node
        current = current->next;
    }
}
void removeFilesAndDirectories(const char* directoryPath) {
    DIR* directory = opendir(directoryPath);
    if (directory == NULL) {
        perror("Error opening directory");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Create the path for the entry
        char entryPath[MAX_FILENAME + strlen(directoryPath)];
        snprintf(entryPath, sizeof(entryPath), "%s/%s", directoryPath, entry->d_name);

        // Get the file status
        struct stat st;
        if (stat(entryPath, &st) == -1) {
            perror("Error getting file stat");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Entry is a directory, recursively remove its contents
            removeFilesAndDirectories(entryPath);
        } else {
            // Entry is a file, remove it
            if (remove(entryPath) != 0) {
                perror("Error removing file");
            }
        }
    }

    closedir(directory);

    // Remove the directory itself
    if (rmdir(directoryPath) != 0) {
        perror("Error removing directory");
    }
}
//Clean every entries in parsedCommand and decoder arrays. They both sized 3 in row
void free2DArray(char** array) {
    for (int i = 0; i < 3; i++) {
        free(array[i]);
    }
    free(array);
}
#endif
