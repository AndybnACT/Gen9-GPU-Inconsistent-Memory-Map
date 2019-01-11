## Kernel Module Development:
### Developing Environment:
- Hardware: intel x64, iommu: off
- kernel version: 4.16.1
- `gcc --version`: gcc (GCC) 8.2.1 20180831
---
#### General
  - **Kernel Page Table Management**:
     - [kernel.org](https://www.kernel.org/doc/gorman/html/understand/understand006.html)
     - Linux Memory management [ppt](http://www.cs.columbia.edu/~krj/os/lectures/L17-LinuxPaging.pdf)
  - Find `task_struct` by pid: 
     - [StackOverflow](https://stackoverflow.com/questions/8547332/kernel-efficient-way-to-find-task-struct-by-pid)
  - Get physical addresses from kernel space: (`follow_pte_pmd`)
     - [StackOverflow](https://stackoverflow.com/questions/5748492/is-there-any-api-for-determining-the-physical-address-from-virtual-address-in-li)
     - [source code](https://elixir.bootlin.com/linux/v4.16.1/source/mm/memory.c#L4335)
  - Program `ioctl` with kernel module(Minimal workable code):
     - [StackOverflow](https://stackoverflow.com/questions/2264384/how-do-i-use-ioctl-to-manipulate-my-kernel-module)
     - [labs](http://www.cs.otago.ac.nz/cosc440/labs/lab06.pdf)
  - OSDev:
     - [Wiki](https://wiki.osdev.org/Main_Page)
  - Necessity of `try_module_get / module_put`:
     - [StackOverflow](https://stackoverflow.com/questions/1741415/linux-kernel-modules-when-to-use-try-module-get-module-put)
  - `kmap_atomic`:
     - [kernel.org](https://www.kernel.org/doc/Documentation/vm/highmem.txt)
     - high memory management [kernel.org](https://www.kernel.org/doc/gorman/html/understand/understand012.html)
  - `seq_file`:
     - [Linux Kernel Module Programming Guide](https://linux.die.net/lkmpg/x861.html)
---
#### PCI
 - Writing to PCI BAR:
    - [StackOverflow](https://stackoverflow.com/questions/35058353/address-mapping-of-pci-memory-in-kernel-space)
    - [Enabling PCI](https://stackoverflow.com/questions/23414288/do-i-need-to-enable-a-pcie-memory-region-in-a-linux-3-12-driver)
---
### Notes for Kernel Module: 
 - Permission of module parameters:
    - [StackOverflow](https://stackoverflow.com/questions/27480369/why-should-the-permisson-attrbute-be-specified-for-every-variable-declared-in-ke)
    - [bootlin](https://elixir.bootlin.com/linux/v4.16.1/source/include/linux/moduleparam.h#L128)


### Lectures:
 - **kernel module programming guide** :
    - [pdf](https://www.tldp.org/LDP/lkmpg/2.4/lkmpg.pdf)
 - **Essential Linux Device Driver**:
    - [selling page](http://www.elinuxdd.com/~elinuxdd/elinuxdd.docs/samplechapters.html)
 - **linux Kernel Workbook**:
    - [Online Doc](https://lkw.readthedocs.io/en/latest/doc/00_about_the_book.html)
 - **Linux Inside**:
    - [GitBook](https://0xax.gitbooks.io/linux-insides/content/)
 - Implementing `ioctl` without an argument:
    - [Blog](http://tuxthink.blogspot.com/2011/01/creating-ioctl-command.html)
 - Memory management in kernel:
    - [Blog](https://manybutfinite.com/post/how-the-kernel-manages-your-memory/)
 - Goto statement:
    - [Blog](https://manybutfinite.com/post/goto-and-the-folly-of-dogma/)
 - Define or declare functions in header files:
    - [StackOverflow](https://softwareengineering.stackexchange.com/questions/56215/why-can-you-have-the-method-definition-inside-the-header-file-in-c-when-in-c-y)
 - Static functions:
    - [StackOverflow](https://stackoverflow.com/questions/558122/what-is-a-static-function)
    
    
### Documentation:
 - Linux:
    - [4.15.0-Gitlab](https://linux-kernel-labs.github.io/master/)
 - Intel Gen9 GPUs:
    - [Doc Category](https://01.org/linuxgraphics/documentation/hardware-specification-prms)
    - [Architecture PDF](https://software.intel.com/sites/default/files/managed/c5/9a/The-Compute-Architecture-of-Intel-Processor-Graphics-Gen9-v1d0.pdf)
    - [Intel Gen9 Memory Management](https://01.org/sites/default/files/documentation/intel-gfx-prm-osrc-skl-vol05-memory_views.pdf)
    - [DRM Memory Management PDF](https://01.org/linuxgraphics/gfx-docs/drm/gpu/drm-mm.html)
        - GEM documentation: [LWN](https://lwn.net/Articles/283798/)
        
        
### Useful:
 - Linux error codes:
    - [/usr/include/asm/errno.h](http://www-numi.fnal.gov/offline_software/srt_public_context/WebDocs/Errors/unix_system_errors.html)