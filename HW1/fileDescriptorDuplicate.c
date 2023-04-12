#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int myDup(int oldfd){
    return fcntl(oldfd, F_DUPFD);
}

int myDup2(int oldfd, int newfd){

    if(oldfd == newfd){
        /* When oldfd is not a valid file descriptor */
        if(fcntl(oldfd,F_GETFL) == -1){
            errno = EBADF;
            return -1;
        }
        return newfd; 
        //return fcntl(oldfd, F_DUPFD, newfd);
    }

    else{
        /* When oldfd is not a valid file descriptor */
        if(fcntl(oldfd,F_GETFL) == -1){
            errno = EBADF;
            return -1;
        }
        /* When newfd was previously open */
        if(fcntl(newfd,F_GETFL) != -1){
            close(newfd);
        }

        fcntl(oldfd, F_DUPFD, newfd);
        return newfd;

    }

}


int main(){

    char * buffer_fd = "This line comes from fd\n";
    char * buffer_fd_dup = "This line comes from dup()\n";
    char * buffer_fd_dup2 = "This line comes from dup2()\n";

    /* TEST 1 - dup()  */
    printf("\n\tTEST 1 - dup()\n");
    int fd_test1 = open("part2_test1.txt", O_CREAT | O_WRONLY, 0666);
    int test1 = myDup(fd_test1);
    printf("fd is %d, the fd returned by dup() is %d.\n",fd_test1,test1);
    write(fd_test1, buffer_fd, strlen(buffer_fd));
    write(test1, buffer_fd_dup, strlen(buffer_fd_dup));

    /* TEST 2 - dup2(), when oldfd is invalid */
    printf("\n\tTEST 2 - dup2(), when oldfd is invalid\n");
    int fd_test2 = open("part2_test.txt", O_CREAT | O_WRONLY, 0666);
    int test2 = myDup2(500,fd_test2);
    printf("fd is %d, the fd returned by dup2() is %d.\n",fd_test2,test2);
    
    /* TEST 3 - dup2(), when oldfd and newfd are equal (oldfd is not valid) */
    printf("\n\tTEST 3 - dup2(), when oldfd and newfd are equal (oldfd is not valid)\n");
    int fd_test3 = open("part2_test.txt", O_CREAT | O_WRONLY, 0666);
    int test3 = myDup2(500,500);
    printf("fd is %d, the fd returned by dup2() is %d.\n",fd_test3,test3);

    /* TEST 4 - dup2(), when oldfd and newfd are equal (oldfd is valid) */
    printf("\n\tTEST 4 - dup2(), when oldfd and newfd are equal (oldfd is valid)\n");
    int fd_test4 = open("part2_test4.txt", O_CREAT | O_WRONLY, 0666);
    int test4 = myDup2(fd_test4,fd_test4);
    printf("fd is %d, the fd returned by dup2() is %d.\n",fd_test4,test4);
    write(fd_test4, buffer_fd, strlen(buffer_fd));
    write(test4, buffer_fd_dup2, strlen(buffer_fd_dup2));

    /* TEST 5 - dup2(), when newfd is valid */
    printf("\n\tTEST 5 - dup2(), when newfd is valid\n");
    int fd_test5 = open("part2_test.txt", O_CREAT | O_WRONLY, 0666);
    int test5 = myDup2(fd_test5,fd_test5);
    printf("fd is %d, the fd returned by dup2() is %d.\n",fd_test5,test5);

    return 0;
}