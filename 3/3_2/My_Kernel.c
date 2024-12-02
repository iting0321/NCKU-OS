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
    struct my_device_data *my_data = (struct my_device_data *) file->private_data;
    ssize_t len = min(my_data->size - *offset, buffer_len);
    if(len <= 0) return 0;
   
    if (copy_from_user(my_data->buffer+ *offset, ubuf, len)) {
        pr_err("Failed to copy data from user space\n");
        return -EFAULT;
    }

    *offset += to_copy ; 
    pr_info("Received from user: %s\n", buf);
    return to_copy;
}


static ssize_t Myread(struct file *fileptr, char __user *ubuf, size_t buffer_len, loff_t *offset) {
    
    struct task_struct *thread;
    struct my_device_data *my_data = (struct my_device_data *) file->private_data;
    ssize_t len = min(my_data->size - *offset, buffer_len);

    // Check if the offset is greater than zero (data has already been read)
    if (*offset > 0) {
        return 0; // EOF
    }
    if(len<=0) return 0;
    
    // Iterate through all threads of the current process
    for_each_thread(current, thread) {
        if(current->pid==thread->pid) continue;
        len += sprintf(buf + len, "PID: %d, TID: %d, time: %d\n",current->pid,
                       thread->pid, thread->utime);
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
