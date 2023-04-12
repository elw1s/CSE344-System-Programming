#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>


int isNumber(char number[])
{
    int i = 0;

    if (number[0] == '-')
        i = 1;
    for (; number[i] != 0; i++)
    {
        if (!isdigit(number[i]))
            return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {


    int isXgivenToTheProgram = 0;

    if(argc == 4){

        if(strcmp(argv[3], "x") == 0){
            isXgivenToTheProgram = 1;
        }
        else{
            printf("\nThe argument %s is not recognized.\n", argv[3]);
            exit(1);
        }

    }

    if(argc <= 4){

        if(isNumber(argv[2])){
            if(isXgivenToTheProgram == 0){
                int fd = open(argv[1], O_CREAT | O_WRONLY | O_APPEND, 0666);
                
                if(fd == -1){
                    
                    if(fd == -1)

                    printf("Error while opening the file\n");
                    exit(1);
                }
            
                int numberOfBytes = atoi(argv[2]);
                char* byte = (char *) malloc(1 * sizeof(char));

                for(int i = 0; i < numberOfBytes; i++){
                    if(write(fd,byte,1)== -1){
                        printf("Error while writing into the file\n");
                        exit(1);
                    }
                }


                if(close(fd) == -1){
                    printf("Error while closing the file\n");
                    exit(1);
                }
            }
            else{
                int fd = open(argv[1], O_CREAT | O_WRONLY, 0666);

                if(fd == -1){
                    printf("Error while opening the file\n");
                    exit(1);
                }
                int numberOfBytes = atoi(argv[2]);
                char* byte = (char *) malloc(1 * sizeof(char));

                for(int i = 0; i < numberOfBytes; i++){
                    lseek(fd,0,SEEK_END);
                    if(write(fd,byte,1)== -1){
                        printf("Error while writing into the file\n");
                        exit(1);
                    }
                }

                if(close(fd) == -1){
                    printf("Error while closing the file\n");
                    exit(1);
                }
            }
        }
        else{
            printf("\nPlease specify number in the second argument as num-bytes\n");
            exit(1);
        }
    }
    else{
        printf("\nArgument limit is exceeded. Please follow the instructions:\n\t./appendMeMore filename num-bytes [x](optional)\n");
        exit(1);
    }

 return 0;   
}