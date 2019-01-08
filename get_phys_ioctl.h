#ifndef IOCTL_H
#define IOCTL_H
#include <linux/ioctl.h>

struct proc_page {
        int pidnr;
        unsigned long vaddr;
        unsigned long pfn;
        unsigned long *ptep;
};

#define get_phys_MAGIC 0xA5
#define get_phys_IOGETLASTCTX _IO(get_phys_MAGIC, 1)
#define get_phys_IOLOOPCTX    _IO(get_phys_MAGIC, 2)
#define get_phys_IOSETVICTIM  _IOW(get_phys_MAGIC, 3, struct proc_page)
#define get_phys_IOSETPPGTT   _IOW(get_phys_MAGIC, 4, unsigned long)
// #define get_phys_IOGETPFN
// #define get_phys_IOSETPFN
#endif