#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
char SECRET_DAT[4096] = "Here is my secret data, blah blah blah\n";


int main(int argc, char const *argv[]) {
    pid_t id;
    id = getpid();
    printf("My pid = %d\n", id);
    while (1){
        printf("%s", SECRET_DAT);
        printf("vma=0x%lx\n", SECRET_DAT);
        // cacheflush(SECRET_DAT, 100, BCACHE);
        sleep(10);
    }
    return 0;
}