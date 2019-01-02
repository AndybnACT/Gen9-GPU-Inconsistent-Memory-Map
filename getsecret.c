#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#define PG_OFFSET_MASK 0xfff

int main(){
	int *a;
	unsigned long offset = 0;
	size_t vma;
    int count = 5;
	int fd = open("/dev/Get_Phys", O_RDONLY);
	if (fd < 0){
		perror("error opening file: ");
		return -1;
	}
    a = (int*) malloc(4096*sizeof(int));
    for (size_t i = 0; i < 4096; i++) {
        a[i] = i;
    }
	printf("vma of a=0x%lx\n", a);
	
    
    vma = a;
	if (pread(fd, &offset, 8, vma) < 0){
		perror("error reading file: ");
		return -1;
	}
    
	vma = ((vma >> 12) << 12 ) | offset;
    while (count--) {
        printf("%s",(char*)vma);
        sleep(5);
    }
	close(fd);
}
