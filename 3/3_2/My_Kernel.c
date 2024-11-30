#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <asm/current.h>

#define procfs_name "Mythread_info"
#define BUFSIZE  1024
char buf[BUFSIZE]; //kernel buffer

static ssize_t Mywrite(struct file *fileptr, const char __user *ubuf, size_t buffer_len, loff_t *offset) {
    if (buffer_len > BUFSIZE - 1) {
        pr_err("Input too large\n");
        return -EINVAL;
    }

    if (copy_from_user(buf, ubuf, buffer_len)) {
        pr_err("Failed to copy data from user space\n");
        return -EFAULT;
    }

    buf[buffer_len] = '\0'; // Null-terminate the string
    pr_info("Received from user: %s\n", buf);
    return buffer_len;
}


static ssize_t Myread(struct file *fileptr, char __user *ubuf, size_t buffer_len, loff_t *offset) {
    int len = 0;

    // Fill the buffer with the thread information
    len += snprintf(buf, BUFSIZE,
                    "Process ID: %d "
                    "Thread Group ID: %d "
                    "Priority: %d "
                    "State: %ld\n",
                    current->pid,              // Process ID
                    current->tgid,             // Thread Group ID
                    current->prio,             // Priority
                    current->state);           // State

    // Copy data to user space
    if (*offset > 0 || buffer_len < len) {
        return 0; // End of file or buffer too small
    }

    if (copy_to_user(ubuf, buf, len)) {
        pr_err("Failed to copy data to user space\n");
        return -EFAULT;
    }

    *offset = len; // Update the offset
    return len;    // Return the number of bytes read
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
