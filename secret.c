#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
char DAT[4096] = "Here is my secret data, blah blah blah\n";


int main(int argc, char const *argv[]) {
    char *SECRET = aligned_alloc(4096, 4096);
    pid_t id;
    
    memcpy(SECRET, DAT, 2048);
    id = getpid();
    printf("My pid = %d\n", id);
    while (1){
        printf("%s", SECRET);
        printf("vma=0x%lx\n", SECRET);
        // cacheflush(SECRET_DAT, 100, BCACHE);
        sleep(10);
    }
    return 0;
}