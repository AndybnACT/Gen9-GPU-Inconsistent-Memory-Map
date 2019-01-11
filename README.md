# Implementing Inconsistent Memory Mapping on Intel's Gen9 GPU 
## Working Environment:
 - Hardware:
    - CPU: Intel Core i5-6400 CPU @ 2.70GHz
    - GPU: Intel® HD Graphics 530 (Gen9)
    - IOMMU(VT-d): off
 - Software Stack:
    - Linux kernel: 4.16.1
    - Device driver: i915, (GEM)
    - OpenCL Library: Neo OpenCL runtime
 - Terms:
    - `pte`: page translation entry
    - `pfn`: page frame number
    - `kernel`: context running on Intel GPU 
    
## Components of this repository:
 1. Victim process:
    - The process owns a secret string allocated at 4K page boundary. We chose to allocate at page boundary in order not to play around with offsets.
    - Once executed, the process tells its pid and enters a infinite loop printing virtual address and the content of that secret string.
 2. Malicious process:
    - The process has two components. A controlling process which runs on CPU and an openCL context which performs the seemingly harmless R/W.
    - The process will pause at several points making it available for user to inspect the page table of the GPU through `/proc/ppgtt` at different stage.
 3. Malicious kernel module:
    - The kernel module exposes several `ioctl` interfaces to cooperate with the malicious process finding and setting `ptes`.
    - The kernel module is not thread-safed, and does not prevent user from accessing it simultaneously.  
    
    
## Steps
 1. Check if the version of linux kernel and intel GPU matches.
 2. Download the kernel source and include it in the Makefile
 3. Compile the kernel module.
 4. Insert the module and create a corresponding device file.
 5. Compile and run the victim process.
 6. Compile the malicious program according to the printed pid and vma.
 7. Run the malicious program.
 8. Magic!
 
 
## Technical details:
 - The implementation does not follow all steps provided from the paper. The differences are:
    - The essential step of the attack is to find the `pte` of dummy buffer in GPU. However, instead of finding it by getting the `pfn` at the first place and matching through the whole page tables, we enqueue a `kernel` context to get its virtual address then walk though the page table to get the target page.
    