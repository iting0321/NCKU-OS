#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <asm/current.h>

#define procfs_name "Mythread_info"
#define BUFSIZE  1024
char buf[BUFSIZE];

static ssize_t Mywrite(struct file *fileptr, const char __user *ubuf, size_t buffer_len, loff_t *offset){
    /* Do nothing */
	return 0;
}


static ssize_t Myread(struct file *fileptr, char __user *ubuf, size_t buffer_len, loff_t *offset) {
    int len = 0;

    // Only write data on the first read
    if (*offset > 0) {
        return 0; // EOF
    }

    // Fill the kernel buffer using sprintf
    len += sprintf(buf + len, "Process ID: %d\n", current->pid);
    len += sprintf(buf + len, "Thread ID: %d\n", current->tgid);
    len += sprintf(buf + len, "Priority: %d\n", current->prio);
    len += sprintf(buf + len, "State: %ld\n", current->__state);

    // Check if the user buffer is large enough
    if (buffer_len < len) {
        return -EINVAL; // Invalid argument
    }

    // Copy data to user buffer
    if (copy_to_user(ubuf, buf, len)) {
        return -EFAULT; // Bad address
    }

    // Update the offset to indicate data has been read
    *offset += len;

    return len; // Return the number of bytes read
}

static struct proc_ops Myops = {
    .proc_read = Myread,
    .proc_write = Mywrite,
};

static int My_Kernel_Init(void){
    proc_create(procfs_name, 0644, NULL, &Myops);   
    pr_info("My kernel says Hi");
    return 0;
}

static void My_Kernel_Exit(void){
    pr_info("My kernel says GOODBYE");
}

module_init(My_Kernel_Init);
module_exit(My_Kernel_Exit);

MODULE_LICENSE("GPL");