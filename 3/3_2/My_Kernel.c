#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <asm/current.h>

#define procfs_name "Mythread_info"
#define BUFSIZE  2048
char buf[BUFSIZE]; //kernel buffer

static unsigned int procfs_buffer_size = 0;
static ssize_t Mywrite(struct file *fileptr, const char __user *ubuf, size_t buffer_len, loff_t *offset) {   

    pr_info("mywrite start\n");
    procfs_buffer_size = buffer_len; 
    if (buffer_len > BUFSIZE - 1) {
        pr_err("Input too large\n");
        return -EINVAL;
    }
    if (procfs_buffer_size >= BUFSIZE) 
        procfs_buffer_size = BUFSIZE - 1; 

    if (copy_from_user(buf, ubuf, buffer_len)) {
        pr_err("Failed to copy data from user space\n");
        return -EFAULT;
    }
    pr_info("buffer_len %zu\n",buffer_len);
    buf[25] = '\0'; // Null-terminate the string
    pr_info("buf : %s\n",buf);
    //return 0;
    *offset += procfs_buffer_size ; 
    //return 0;
    pr_info("Received from user: %s\n", buf);
    return procfs_buffer_size  ;
}


static ssize_t Myread(struct file *fileptr, char __user *ubuf, size_t buffer_len, loff_t *offset) {
    struct task_struct *thread;
    ssize_t len = procfs_buffer_size; 
    
    pr_info("buffer size %zu\n",buffer_len);
    // Check if the offset is greater than zero (data has already been read)
    if (*offset > 0) {
        return 0; // EOF
    }
    
    len += sprintf(buf + len, "PID: %d, TID: %d, time: %d\n",current->tgid,
                        current->pid, current->utime);

    // Ensure we have enough space in the user buffer
    if (!ubuf || buffer_len < len) {
        pr_err("Invalid user buf\n");
        return -EINVAL; // Error: Invalid argument
    }

    // Copy the data to the user buffer
    if (copy_to_user(ubuf, buf, len)) {
	pr_err("fail to copy\n");
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
    proc_create(procfs_name, 0777, NULL, &Myops);   
    pr_info("My kernel says Hi");
    return 0;
}

static void My_Kernel_Exit(void){
    pr_info("My kernel says GOODBYE");
}

module_init(My_Kernel_Init);
module_exit(My_Kernel_Exit);

MODULE_LICENSE("GPL");
