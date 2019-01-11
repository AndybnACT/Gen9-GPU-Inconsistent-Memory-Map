#include <linux/kernel.h> // container_of
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h> // task_struct,  get_pid_task
#include <asm/current.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <asm/pgtable.h> // pte_pfn

// for character device implementation
#include <linux/fs.h>
#include <linux/cdev.h>
// ioctl
#include "get_phys_ioctl.h"


// for accessing user space pointers
#include <asm/uaccess.h>

// for mapping intel graphics devices 
#include <linux/pci.h>// pci_dev
#include "i915_drv.h" // drm_i915_private
#include "i915_gem_context.h" // i915_gem_context
#include "intel_device_info.h"// device info
#include "i915_gem_gtt.h" // i915 page walk


// for dumpping gen8 ppgtt
// -> seq_file
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_LICENSE("Dual BSD/GPL");
static dev_t DEVID;
#define DEVCOUNT 1

// static int nr = 0;
// module_param(nr, int, S_IRUGO);
// static unsigned long vaddress = 0x0;
// module_param(vaddress, ulong, S_IRUGO);

#define PG_OFFSET_MASK 0xfff

// static unsigned long secret_pfn;




static unsigned int i915_vid = 0x8086;
static unsigned int i915_did = 0x1912; 


struct drm_i915_private *i915_priv = NULL;
struct i915_gem_context *i915_last_ctx = NULL;

struct get_phys_private {
        struct  cdev cdev;
        struct proc_page victim_cpu;
        struct proc_page dummy_gpu;
        pid_t pid;
        unsigned long vma;
        unsigned long pfn;
        pte_t *victim_ptep;
        pte_t saved_pte;
};
struct get_phys_private devs[DEVCOUNT];



//=================== FILE OPERATION BLOCK =================
// IOCTL

static struct i915_gem_context* get_last_ctx(struct drm_i915_private *i915){
        struct list_head *head = &(i915->contexts.list);
        struct i915_gem_context *ret = NULL;
        // get prev ctx, which is the last ctx
        if (head->prev == head) {
                printk(KERN_ALERT "Get_Phys: context not found!!");
                return NULL;
        }
        ret = container_of(head->prev, struct i915_gem_context, link);
        return ret;
}
static void loop_ctx(struct drm_i915_private *i915){
        struct list_head *head = &(i915->contexts.list);
        struct list_head *iter;
        list_for_each(iter, head){
                struct i915_gem_context *ctx = list_entry(iter, struct i915_gem_context, link);
                printk(KERN_WARNING "Get_Phys: %s\n", ctx->name); 
        }
        return;
}

static int get_pfn_from_proc(struct proc_page *userproc){
        struct task_struct *task;
        spinlock_t *ptl;
        pte_t *ptep;
        int res;

        int nr = userproc->pidnr;
        task = get_pid_task(find_vpid(nr), PIDTYPE_PID);
        if (!task) {
                printk(KERN_NOTICE "Get_Phys: task not found\n");
                return -EFAULT;
        }
        
        res = follow_pte_pmd(task->mm, userproc->vaddr, NULL, NULL, &ptep, NULL, &ptl );
        if (res) {
                printk(KERN_NOTICE "Get_Phys: PFN NOT FOUND\n");
                return -EFAULT;
        }
        userproc->pfn = pte_pfn(*ptep);
        pte_unmap_unlock(ptep, ptl);
        printk("Get_Phys: PFN found %lx", userproc->pfn);
        return 0;
}
static struct i915_page_table* ctx_find_pt(u64 va){
        u64 i915_pml4e = gen8_pml4e_index(va);
        u32 i915_pdpe  = gen8_pdpe_index(va);
        u32 i915_pde   = gen8_pde_index(va);
        struct i915_page_directory_pointer *pdp;
        struct i915_page_directory *pd;
        struct i915_page_table *pt;
        struct i915_hw_ppgtt *ppgtt = i915_last_ctx->ppgtt;
        struct i915_pml4 *pml4;
        
        printk(KERN_NOTICE "Get_Phys: [%p] Perfoming page walk on %s, total=%lld\n", i915_last_ctx,
                                i915_last_ctx->name, ppgtt->base.total);
        // get pml4 table
        pml4 = &ppgtt->pml4;
        // get pdp table
        if (i915_pml4e >= GEN8_PML4ES_PER_PML4 ) {
            return NULL;
        }
        pdp = pml4->pdps[i915_pml4e];
        // get pd table
        if (pdp == ppgtt->base.scratch_pdp || i915_pdpe >= pdp->used_pdpes) {
            printk(KERN_NOTICE "Get_Phys: Bad address pdp=%p, pdpe=%d, used=%d\n",
                                pdp, i915_pdpe, pdp->used_pdpes);
            return NULL;
        }
        pd = pdp->page_directory[i915_pdpe];
        // get page table
        if (pd == ppgtt->base.scratch_pd || i915_pde >= pd->used_pdes) {
            printk(KERN_NOTICE "Get_Phys: Bad address pd=%p, pde=%d, used=%d\n",
                                pd, i915_pde, pd->used_pdes);
            return NULL;
        }
        pt = pd->page_table[i915_pde];
        if (pt == ppgtt->base.scratch_pt) {
            printk(KERN_NOTICE "Get_Phys: Bad address pt=%p\n", pt);
            return NULL;
        }
        printk(KERN_NOTICE "Get_Phys: pml4e=%lld, pdpe=%d pde=%d\n", i915_pml4e, i915_pdpe, i915_pde);
        

        return pt;
}
static int set_ctx_pte(struct get_phys_private *priv){
        u64 i915_va = priv->dummy_gpu.vaddr;
        u32 i915_pte = gen8_pte_index(i915_va);
        u64 evil_pte = 0x0;
        struct i915_page_table *page_table;
        gen8_pte_t *pt_vaddr=NULL;
        // gen8_pte_t *direct_access_ptr=NULL;
        // gen8_pte_t *found_ptep;
        int tmpi;
        
        
        page_table = ctx_find_pt(i915_va);
        if (!page_table) 
                return -EFAULT;
        // direct_access_ptr = page_table->base.page->virtual;
        // if (direct_access_ptr) {
        //         for (tmpi = 0; tmpi < 512; tmpi+=4) {
        //                 printk(KERN_NOTICE "D_access!  Get_Phys: %llx   %llx   %llx  %llx\n", direct_access_ptr[tmpi],
        //                         direct_access_ptr[tmpi+1], direct_access_ptr[tmpi+2] , direct_access_ptr[tmpi+3]);
        //         }
        // }
        pt_vaddr = kmap_atomic((&(page_table)->base)->page);        
        // found_ptep = pt_vaddr+i915_pte;
        printk(KERN_NOTICE "Get_Phys: found pte[%d] = %llx", i915_pte, pt_vaddr[i915_pte]);
        for (tmpi = 0; tmpi <= i915_pte; tmpi+=4) {
                printk(KERN_NOTICE "Get_Phys: %llx   %llx   %llx  %llx\n", pt_vaddr[tmpi], pt_vaddr[tmpi+1], pt_vaddr[tmpi+2] , pt_vaddr[tmpi+3]);
        }
        // prepare the inconsistent map
        evil_pte = priv->victim_cpu.pfn << 12;
        evil_pte |= 0x83;
        pt_vaddr[i915_pte] = evil_pte;
        // writeq(evil_pte, pt_vaddr+i915_pte);
        printk(KERN_NOTICE "Get_Phys: Done setting pppgtt, pte = %llx\n", evil_pte);
        kunmap_atomic(pt_vaddr);
        return 0;
}

static long get_phys_ioctl(struct file *filep, unsigned int op, unsigned long argp){
        void __user *arg_user;
        struct get_phys_private *gppriv;
        gppriv = (struct get_phys_private*) filep->private_data;
        
        printk(KERN_WARNING "Get_Phys: User space program perfoms ioctl\n");
        arg_user = (void __user *) argp;
        switch (op) {
                case get_phys_IOGETLASTCTX:
                        i915_last_ctx = get_last_ctx(i915_priv);
                        if (!i915_last_ctx) {
                                return -EFAULT;
                        }
                        printk(KERN_WARNING "Get_Phys: gets last context: %s\n", i915_last_ctx->name);
                        break;
                case get_phys_IOLOOPCTX:
                        loop_ctx(i915_priv);
                        break;
                case get_phys_IOSETVICTIM:
                        if (copy_from_user(&gppriv->victim_cpu, arg_user, 
                                            sizeof(struct proc_page)))
                                return -EFAULT;
                        if(get_pfn_from_proc(&gppriv->victim_cpu))
                                return -EINVAL;
                        break;
                case get_phys_IOSETPPGTT:
                        if (copy_from_user(&gppriv->dummy_gpu.vaddr, arg_user,
                                            sizeof(unsigned long)))
                                return -EFAULT;
                        if (set_ctx_pte(gppriv)) {
                            return -ENOSYS;
                        }
                        break;
                default:
                        return -EINVAL;
        }
        return 0;
}


static int get_phys_open(struct inode *inode, struct file *file){
        struct get_phys_private *gppriv;
        gppriv = container_of(inode->i_cdev, struct get_phys_private, cdev);
        
        file->private_data = gppriv;
        printk(KERN_NOTICE "Get_Phys: userspace program opens the device\n");
        
        
        return 0;
}
// static ssize_t get_phys_read(struct file *file, char __user *user_buffer, size_t size, loff_t *offset){
//         struct get_phys_private *gppriv;
//         spinlock_t *ptl;
//         pte_t *ptep;
//         pte_t dummy_pte;
//         ssize_t ret = sizeof(unsigned long);
//         unsigned long victim_offset;
// 
//         printk(KERN_NOTICE "Get_Phys: userspace program reads the device\n");
//         gppriv = (struct get_phys_private *) file->private_data;
//         gppriv->pid = current->pid;
//         gppriv->vma = *offset;
// 
//         // get physical address
//         ret = follow_pte_pmd(current->mm, gppriv->vma, NULL, NULL, &ptep, NULL, &ptl );
//         if (ret) {
//                 printk(KERN_ALERT "Get_Phys: PFN NOT FOUND\n");
//                 return -1;
//         }
// 
//         gppriv->pfn = pte_pfn(*ptep);
//         pte_unmap_unlock(ptep, ptl);
//         gppriv->victim_ptep = ptep;
//         gppriv->saved_pte.pte = ptep->pte;
//         printk(KERN_NOTICE "Get_Phys: PFN Found: 0x%lx\n", gppriv->pfn);
// 
//         // do remapping
//         dummy_pte.pte = (secret_pfn << PAGE_SHIFT) & PTE_PFN_MASK;
//         ptep->pte = ptep->pte & (~PTE_PFN_MASK);
//         ptep->pte = ptep->pte | dummy_pte.pte;
// 
// 
//         victim_offset = vaddress&PG_OFFSET_MASK;
// 
// 
//         if (copy_to_user(user_buffer, &victim_offset, sizeof(unsigned long))) {
//                 return -EFAULT;
//         }
// 
//         return ret;
// }
static int get_phys_release(struct inode *inode, struct file *file){
    struct get_phys_private *gppriv;
    printk("Get_Phys: userspace program releases the device\n");
    gppriv = (struct get_phys_private *) file->private_data;
    // gppriv->victim_ptep->pte = gppriv->saved_pte.pte;
    i915_last_ctx = NULL;
    return 0;
}
const struct file_operations get_phys_fops = {
        .owner = THIS_MODULE,
        .open  = get_phys_open,
        .release = get_phys_release,
        .unlocked_ioctl = get_phys_ioctl
};
//=================== FILE OPERATION BLOCK =================



// =================== seq_file ===================
struct seq_private {
        struct i915_gem_context *cur_ctx;
        struct list_head *head;
        struct list_head *iter;
};

static DEFINE_MUTEX(seq_mutex);
union get_phys_seq_state {
	struct {
		unsigned short pos;
		bool mutex_acquired;
	};
	void *p;
};
static struct i915_gem_context *seq_cur_ctx;
static struct list_head *seq_head;
static struct list_head *seq_iter;
static void *gp_seq_start(struct seq_file *s, loff_t *pos){
        union get_phys_seq_state *state = (union get_phys_seq_state *) &s->private;
        int err;
        
        BUILD_BUG_ON(sizeof(union get_phys_seq_state) != sizeof(s->private));
        
        err = mutex_lock_interruptible(&seq_mutex);
        if (err) {
                state->mutex_acquired = false;
                return ERR_PTR(err);
        }
        state->mutex_acquired = true;
        
        seq_head = &i915_priv->contexts.list;
        
        return seq_list_start(seq_head, *pos);
        
        
        
    	// // static unsigned long counter = 0;
        // printk(KERN_NOTICE "seq: printing information\n");
        // /* beginning a new sequence ? */	
        // if (*pos == 0) {
        //         /* yes => return a non null value to begin the sequence */
        //         // seq_printf(s, "st\n");
        //         seq_head = &(i915_priv->contexts.list);
        //         seq_iter = seq_head->next;
        //         seq_cur_ctx = container_of(seq_iter, struct i915_gem_context, link);
        //         return seq_cur_ctx;
        // }else{
        //         if (seq_iter->next == seq_head) {
        //                 printk(KERN_NOTICE "seq: ed\n");
        //                 /* no => it's the end of the sequence, return end to stop reading */
        //                 *pos = 0;
        //                 return NULL;
        //         }else{
        //             printk(KERN_NOTICE "seq: restart\n");
        //             return seq_cur_ctx;
        //         }
        // 
        // }
}

/**
 * This function is called after the beginning of a sequence.
 * It's called untill the return is NULL (this ends the sequence).
 *
 */
static void *gp_seq_next(struct seq_file *s, void *v, loff_t *pos){
        union get_phys_seq_state *state = (union get_phys_seq_state *) &s->private;
        state->pos = *pos + 1;
        return seq_list_next(v, seq_head, pos);
        // if (seq_iter->next == seq_head) {
        //         printk(KERN_NOTICE "seq: stop next\n");
        //         return NULL;
        // }else{
        //         printk(KERN_NOTICE "seq: next\n");
        //         seq_iter = seq_iter->next;
        //         seq_cur_ctx = container_of(seq_iter, struct i915_gem_context, link);
        //         *pos += 1;
        //         v = seq_cur_ctx;
        //         return v;
        // }
}

static void gp_seq_stop(struct seq_file *s, void *v){
        union get_phys_seq_state *state = (union get_phys_seq_state*) &s->private;
        printk(KERN_NOTICE "seq: Done!\n");
        if (state->mutex_acquired) 
                mutex_unlock(&seq_mutex);
}

static int gp_seq_show(struct seq_file *s, void *v){
        // struct i915_hw_ppgtt *ppgtt;
        struct i915_gem_context *ctx = container_of(v, struct i915_gem_context, link);
        printk(KERN_NOTICE "seq: printing\n");
        if (ctx) {
                seq_printf(s, "[%p] context name: %s\n", ctx, ctx->name);
                if (ctx->ppgtt) {
                        seq_printf(s, "dumpping ppgtt\n");
                        ctx->ppgtt->debug_dump(ctx->ppgtt, s);
                }
        }else{
                seq_printf(s, "context not set\n");
        }
        // if (seq_cur_ctx) {
        //         seq_printf(s, "[%p]context name: %s\n", seq_cur_ctx, seq_cur_ctx->name);
        //         ppgtt = seq_cur_ctx->ppgtt;
        //         if (ppgtt) {
        //             seq_puts(s, "found ppgtt\n");
        //             if (ppgtt->debug_dump) {
        //                 seq_puts(s, "dumpping ppgtt\n");
        //                 ppgtt->debug_dump(ppgtt,s);
        //             }
        //         }
        // }else{
        //         seq_printf(s, "context not set\n");
        // }
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
        // struct task_struct *task;
        struct proc_dir_entry *entry;
        int i;
        // spinlock_t *ptl;
        // pte_t *ptep;
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
        entry = proc_create("ppgtt", 666, NULL, &gp_file_ops);
        if (!entry) 
                return -ENOMEM;
        
        
        
        // // get task struct in order to get process's address space
        // task = get_pid_task(find_vpid(nr), PIDTYPE_PID);
        // printk(KERN_ALERT "process being inspected: %d\n", task->pid);
        // printk(KERN_ALERT "process's virtual addr: 0x%lx\n", vaddress);
        // 
        // // get physical address
        // ret = follow_pte_pmd(task->mm, vaddress, NULL, NULL, &ptep, NULL, &ptl );
        // if (ret) {
        //         printk(KERN_ALERT "PFN NOT FOUND\n");
        //         return -1;
        // }
        // // 
        // secret_pfn = pte_pfn(*ptep);
        // pte_unmap_unlock(ptep, ptl); // intel just do an nop for unmapping ptep
        // printk(KERN_NOTICE "Get_Phys: Victim PFN = 0x%lx\n", secret_pfn);
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

