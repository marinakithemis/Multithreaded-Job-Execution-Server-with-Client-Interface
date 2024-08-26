#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <number>\n", argv[0]);
        return 1;
    }
    int delay = atoi(argv[1]);
    if (delay <= 0) {
        printf("Invalid number: %s\n", argv[1]);
        return 1;
    }
    for (int i = 0; i < delay; i++) {
        sleep(1);
        printf("$");
        fflush(stdout);
    }
    printf("\n");
    return 0;
}