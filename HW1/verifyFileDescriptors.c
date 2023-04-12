#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int verifyDuplicatedFileDescriptor(int fd1 , int fd2){
    struct stat stat1, stat2;
    if(fstat(fd1, &stat1) < 0) return -1;
    if(fstat(fd2, &stat2) < 0) return -1;
    int isSameFile = (stat1.st_dev == stat2.st_dev) && (stat1.st_ino == stat2.st_ino);
    if((ftell(fdopen(fd1, "a")) == ftell(fdopen(fd2, "a"))) && (isSameFile)){
        return 1;
    }

    return -1;

}

int main(){

    /* TEST 1 - Same file, different offset */
    printf("\n\tTEST 1 - Same file, offset is set using lseek differently. (Second file descriptor created using dup()\n");
    int test1_1 = open("part3_a.txt", O_CREAT | O_WRONLY, 0666);
    int test1_2 = dup(test1_1);
    lseek(test1_2,10,SEEK_SET);
    lseek(test1_1,5,SEEK_CUR);
    printf("Is duplicated file descriptors share = %d\n",verifyDuplicatedFileDescriptor(test1_1, test1_2));

    /* TEST 2 - Different file, same offset */
    printf("\n\tTEST 2 - Different file, same offset\n");
    int test2_1 = open("part3_a.txt", O_CREAT | O_WRONLY, 0666);
    int test2_2 = open("part3_b.txt", O_WRONLY, 0666);
    lseek(test2_1,0,SEEK_SET);
    lseek(test2_2,0,SEEK_SET);
    printf("Is duplicated file descriptors share = %d\n",verifyDuplicatedFileDescriptor(test2_1, test2_2));

    /* TEST 3 - Same file, same offset */
    printf("\n\tTEST 3 - Same file, same offset\n");
    int test3_1 = open("part3_a.txt", O_CREAT | O_WRONLY, 0666);
    int test3_2 = dup(test3_1);
    lseek(test3_1,0,SEEK_SET);
    lseek(test3_2,0,SEEK_SET);
    printf("Is duplicated file descriptors share = %d\n",verifyDuplicatedFileDescriptor(test3_1, test3_2));

    /* TEST 4 - Different file, different offset */
    printf("\n\tTEST 4 - Different file, different offset\n");
    int test4_1 = open("part3_a.txt", O_CREAT | O_WRONLY, 0666);
    int test4_2 = open("part3_b.txt", O_CREAT | O_WRONLY, 0666);
    lseek(test4_1,5,SEEK_SET);
    lseek(test4_2,10,SEEK_CUR);
    printf("Is duplicated file descriptors share = %d\n",verifyDuplicatedFileDescriptor(test4_1, test4_2));

    return 0;
}


