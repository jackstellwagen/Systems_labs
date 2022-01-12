#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv){
    FILE* fp = fopen ("x.txt", "w");
    fprintf(fp, "HELLO");
    char *str = "hello";
    write(STDOUT_FILENO, str, strlen(str));
    return 1;
}

