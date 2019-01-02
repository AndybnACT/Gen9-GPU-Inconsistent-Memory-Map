#include <linux/kernel.h> // container_of
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h> // task_struct,  get_pid_task
#include <asm/current.h>
#include <linux/mm.h>
#include <asm/pgtable.h> // pte_pfn

// for character device implementation
#include <linux/fs.h>
#include <linux/cdev.h>

// for accessing user space pointers
#include <asm/uaccess.h>

// for mapping intel graphics devices 
#include <linux/pci.h>// pci_dev
#include "i915_drv.h" // drm_i915_private
#include "intel_device_info.h"// device info


// for dumpping gen8 ppgtt
// -> seq_file
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_LICENSE("Dual BSD/GPL");
static dev_t DEVID;
#define DEVCOUNT 1


static int nr = 956;
// module_param(nr, int, S_IRUGO);
#define PG_OFFSET_MASK 0xfff
static unsigned long vaddress = 0x55dee6de77e0;
static unsigned long secret_pfn;
// module_param(vaddress, ulong, S_IRUGO);



static unsigned int i915_vid = 0x8086;
static unsigned int i915_did = 0x1912; 
struct drm_i915_private *i915_priv;


struct get_phys_private {
        struct  cdev cdev;
        pid_t pid;
        unsigned long vma;
        unsigned long pfn;
        pte_t *victim_ptep;
        pte_t saved_pte;
};
struct get_phys_private devs[DEVCOUNT];



//=================== FILE OPERATION BLOCK =================
static int get_phys_open(struct inode *inode, struct file *file){
        struct get_phys_private *gppriv;
        gppriv = container_of(inode->i_cdev, struct get_phys_private, cdev);
        
        file->private_data = gppriv;
        printk(KERN_NOTICE "Get_Phys: userspace program opens the device\n");
        
        
        return 0;
}
static ssize_t get_phys_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset){
        struct get_phys_private *gppriv;
        spinlock_t *ptl;
        pte_t *ptep;
        pte_t dummy_pte;
        ssize_t ret = sizeof(unsigned long);
        unsigned long victim_offset;
        
        printk(KERN_NOTICE "Get_Phys: userspace program reads the device\n");
        gppriv = (struct get_phys_private *) file->private_data;
        gppriv->pid = current->pid;
        gppriv->vma = *offset;
        
        // get physical address
        ret = follow_pte_pmd(current->mm, gppriv->vma, NULL, NULL, &ptep, NULL, &ptl );
        if (ret) {
                printk(KERN_ALERT "Get_Phys: PFN NOT FOUND\n");
                return -1;
        }
        
        gppriv->pfn = pte_pfn(*ptep);
        pte_unmap_unlock(ptep, ptl);
        gppriv->victim_ptep = ptep;
        gppriv->saved_pte.pte = ptep->pte;
        printk(KERN_NOTICE "Get_Phys: PFN Found: 0x%lx\n", gppriv->pfn);
        
        // do remapping
        dummy_pte.pte = (secret_pfn << PAGE_SHIFT) & PTE_PFN_MASK;
        ptep->pte = ptep->pte & (~PTE_PFN_MASK);
        ptep->pte = ptep->pte | dummy_pte.pte;
        
        
        victim_offset = vaddress&PG_OFFSET_MASK;
        
        
        if (copy_to_user(user_buffer, &victim_offset, sizeof(unsigned long))) {
                return -EFAULT;
        }
        
        return ret;
}
static int get_phys_release(struct inode *inode, struct file *file){
    struct get_phys_private *gppriv;
    gppriv = (struct get_phys_private *) file->private_data;
    gppriv->victim_ptep->pte = gppriv->saved_pte.pte;
    
    return 0;
}
const struct file_operations get_phys_fops = {
        .owner = THIS_MODULE,
        .read  = get_phys_read,
        .open  = get_phys_open,
        .release = get_phys_release
};
//=================== FILE OPERATION BLOCK =================



// =================== seq_file ===================
static void *gp_seq_start(struct seq_file *s, loff_t *pos){
    	static unsigned long counter = 0;
        /* beginning a new sequence ? */	
        if (*pos == 0) {
                /* yes => return a non null value to begin the sequence */
                return &counter;
        }else{
                /* no => it's the end of the sequence, return end to stop reading */
                *pos = 0;
                return NULL;
        }
}

/**
 * This function is called after the beginning of a sequence.
 * It's called untill the return is NULL (this ends the sequence).
 *
 */
static void *gp_seq_next(struct seq_file *s, void *v, loff_t *pos){
        return NULL;
}

static void gp_seq_stop(struct seq_file *s, void *v){
	   
}

static int gp_seq_show(struct seq_file *s, void *v){
    	seq_printf(s, "printing information\n");
        return 0;
}

static struct seq_operations gp_seq_ops = {
    	.start = gp_seq_start,
    	.next  = gp_seq_next,
    	.stop  = gp_seq_stop,
    	.show  = gp_seq_show
};

static int gp_open(struct inode *inode, struct file *file){
	   return seq_open(file, &gp_seq_ops);
}

static struct file_operations gp_file_ops = {
    	.owner   = THIS_MODULE,
    	.open    = gp_open,
    	.read    = seq_read,
    	.llseek  = seq_lseek,
    	.release = seq_release
};

// =================== seq_file ===================


static int getphys_init(void){
        struct task_struct *task;
        struct proc_dir_entry *entry;
        int i;
        spinlock_t *ptl;
        pte_t *ptep;
        // unsigned long pfn = 0x0;
        int ret = 0x0;
        
        struct pci_dev *pci_i915;
        // struct drm_i915_private *i915_priv;
        printk(KERN_NOTICE "Get_Phys: initializing\n");
        // device registeration
        ret = alloc_chrdev_region(&DEVID, 0, DEVCOUNT, "Get_Phys");
        if (ret < 0) {
                printk(KERN_WARNING "Get_Phys: can't get major number\n");
                return ret;
        }
        
        for (i = 0; i < DEVCOUNT; i++) {
                printk(KERN_NOTICE "Get_Phys: appending device number %d %d\n", MAJOR(DEVID), MINOR(DEVID) );    
                cdev_init(&devs[i].cdev, &get_phys_fops);
                devs[i].cdev.owner = THIS_MODULE;
                ret = cdev_add(&devs[i].cdev, MKDEV(MAJOR(DEVID), i), 1);
                if (ret < 0) {
                        printk(KERN_WARNING "Get_Phys: can't register cdev on %dth device\n", i);
                        return ret;
                }
        }
        entry = proc_create("test_swq_file", 666, NULL, &gp_file_ops);
        if (!entry) 
                return -ENOMEM;
        
        
        
        // get task struct in order to get process's address space
        task = get_pid_task(find_vpid(nr), PIDTYPE_PID);
        printk(KERN_ALERT "process being inspected: %d\n", task->pid);
        printk(KERN_ALERT "process's virtual addr: 0x%lx\n", vaddress);
        
        // get physical address
        ret = follow_pte_pmd(task->mm, vaddress, NULL, NULL, &ptep, NULL, &ptl );
        if (ret) {
                printk(KERN_ALERT "PFN NOT FOUND\n");
                return -1;
        }
        // 
        secret_pfn = pte_pfn(*ptep);
        pte_unmap_unlock(ptep, ptl); // intel just do an nop for unmapping ptep
        printk(KERN_NOTICE "Get_Phys: Victim PFN = 0x%lx\n", secret_pfn);
        // //----------------------
        // 
        // get pci device
        pci_i915  = pci_get_device(i915_vid, i915_did, NULL);
        if (!pci_i915) {
                printk(KERN_ALERT "PCI device not found\n");
                return -1;
        }
        // get drm_i915_private from the pci device
        i915_priv = container_of(pci_i915->dev.driver_data, struct drm_i915_private, drm);
        printk(KERN_ALERT "Get intel i915 device: id:0x%x, gen:0x%x\n", i915_priv->info.device_id, i915_priv->info.gen);
        
        
        
        printk(KERN_NOTICE "Get_Phys: initialization complete\n");
        return 0;
}
module_init(getphys_init);


static void getphys_exit(void){
        int i;
        for (i = 0; i < DEVCOUNT; i++) {
                cdev_del(&devs[i].cdev);
                printk(KERN_NOTICE "Get_Phys: deleting device number: %d %d\n", MAJOR(DEVID), MINOR(DEVID));
        }
        unregister_chrdev_region(DEVID, DEVCOUNT);
        remove_proc_entry("test_swq_file", NULL);
        printk(KERN_ALERT "Get_Phys: Unloading Kernel Module\n");
}
module_exit(getphys_exit);

