#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 4096 // Add this line

int main(){

    FILE * fp = fopen("testDirectory/test.txt","r");
    char buffer[BUFFER_SIZE];

    printf("Started reading..\n");
    fread(buffer, sizeof(char), BUFFER_SIZE - 1, fp);
    printf("Reading finished..\n");
    /* if (bytesRead < 0) {
        perror("fread");
        fclose(readFp);
        return 1;
    } */
    
    if(fp == NULL){
        printf("Null at the end of file\n");
    }
    if(feof(fp)){
        printf("EOF at the end\n");
    }
    

    return 0;
}