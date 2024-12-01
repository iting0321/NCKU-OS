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
    struct task_struct *thread;
    int len = 0;

    // Check if the offset is greater than zero (data has already been read)
    if (*offset > 0) {
        return 0; // EOF
    }

    // Iterate through all threads of the current process
    for_each_thread(current, thread) {
        len += sprintf(buf + len, "PID: %d, TID: %d, Priority: %d, State: %ld\n",current->pid,
                       thread->pid, thread->prio, thread->__state);
        // Check if the buffer length is exceeded
        if (len >= BUFSIZE) {
            break; // Stop if buffer is full
        }
    }

    // Ensure we have enough space in the user buffer
    if (buffer_len < len) {
        return -EINVAL; // Error: Invalid argument
    }

    // Copy the data to the user buffer
    if (copy_to_user(ubuf, buf, len)) {
        return -EFAULT; // Error: Bad address
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
