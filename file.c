#include <linux/fs.h>
#include <linux/uaccess.h>
#include "osfs.h"

/**
 * Function: osfs_read
 * Description: Reads data from a file.
 * Inputs:
 *   - filp: The file pointer representing the file to read from.
 *   - buf: The user-space buffer to copy the data into.
 *   - len: The number of bytes to read.
 *   - ppos: The file position pointer.
 * Returns:
 *   - The number of bytes read on success.
 *   - 0 if the end of the file is reached.
 *   - -EFAULT if copying data to user space fails.
 */
static ssize_t osfs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(filp);
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;
    struct osfs_extent *extent = osfs_inode->extent_list;
    void *data_block;
    ssize_t bytes_read = 0;
    size_t remaining = len;
    size_t offset, to_read;

    // Check for EOF
    if (*ppos >= osfs_inode->i_size)
        return 0;

    // Adjust the read length if it exceeds the file size
    if (*ppos + len > osfs_inode->i_size)
        len = osfs_inode->i_size - *ppos;

    // Traverse extents to read data
    while (remaining > 0 && extent) {
        size_t extent_start = extent->start_block * BLOCK_SIZE;
        size_t extent_end = extent_start + extent->length * BLOCK_SIZE;

        // Skip extents before the current position
        if (*ppos >= extent_end) {
            extent = extent->next;
            continue;
        }

        // Calculate offset within the extent
        offset = (*ppos > extent_start) ? *ppos - extent_start : 0;

        // Determine the number of bytes to read from this extent
        to_read = min(remaining, extent_end - (*ppos + offset));

        // Read data from the extent
        data_block = sb_info->data_blocks + extent->start_block * BLOCK_SIZE + offset;
        if (copy_to_user(buf + bytes_read, data_block, to_read)) {
            pr_err("osfs_read: Failed to copy data to user space\n");
            return -EFAULT;
        }

        // Update counters and positions
        *ppos += to_read;
        bytes_read += to_read;
        remaining -= to_read;
    }

    pr_info("osfs_read: Read %zu bytes from file inode %lu\n", bytes_read, inode->i_ino);
    return bytes_read;
}



/**
 * Function: osfs_write
 * Description: Writes data to a file.
 * Inputs:
 *   - filp: The file pointer representing the file to write to.
 *   - buf: The user-space buffer containing the data to write.
 *   - len: The number of bytes to write.
 *   - ppos: The file position pointer.
 * Returns:
 *   - The number of bytes written on success.
 *   - -EFAULT if copying data from user space fails.
 *   - Adjusted length if the write exceeds the block size.
 */
static ssize_t osfs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(filp);
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;
    struct osfs_extent *extent = osfs_inode->extent_list;
    void *data_block;
    ssize_t bytes_written = 0;
    size_t remaining = len;
    size_t offset, to_write;
    int ret;

    // Traverse the extent list to find the correct extent or allocate a new one
    while (remaining > 0) {
        // Find the last extent or an extent that can accommodate the write
        while (extent && *ppos >= (extent->start_block * BLOCK_SIZE + extent->length * BLOCK_SIZE)) {
            extent = extent->next;
        }

        // Allocate a new extent if necessary
        if (!extent) {
            uint32_t start_block;
            size_t blocks_needed = (remaining + BLOCK_SIZE - 1) / BLOCK_SIZE;

            // Allocate a new extent
            ret = osfs_alloc_extent(sb_info, &start_block, blocks_needed);
            if (ret) {
                pr_err("osfs_write: Failed to allocate extent\n");
                return ret;
            }

            // Add the new extent to the list
            ret = osfs_add_extent(osfs_inode, start_block, blocks_needed);
            if (ret) {
                pr_err("osfs_write: Failed to add extent to inode\n");
                return ret;
            }

            // Set the newly allocated extent as the current extent
            extent = osfs_inode->extent_list;
            while (extent->next) {
                extent = extent->next; // Traverse to the newly added extent
            }
        }

        // Write within the current extent
        offset = *ppos - (extent->start_block * BLOCK_SIZE);
        to_write = min(remaining, extent->length * BLOCK_SIZE - offset);

        data_block = sb_info->data_blocks + extent->start_block * BLOCK_SIZE + offset;
        if (copy_from_user(data_block, buf + bytes_written, to_write)) {
            pr_err("osfs_write: Failed to copy data from user space\n");
            return -EFAULT;
        }

        // Update metadata and counters
        *ppos += to_write;
        bytes_written += to_write;
        remaining -= to_write;
        osfs_inode->i_size = umax(osfs_inode->i_size, *ppos);
        inode->i_size = osfs_inode->i_size;
    }

    osfs_inode->__i_mtime = current_time(inode);
    osfs_inode->__i_ctime = current_time(inode);
    mark_inode_dirty(inode);

    pr_info("osfs_write: Wrote %zu bytes to file inode %lu\n", bytes_written, inode->i_ino);
    return bytes_written;
}


/**
 * Struct: osfs_file_operations
 * Description: Defines the file operations for regular files in osfs.
 */
const struct file_operations osfs_file_operations = {
    .open = generic_file_open, // Use generic open or implement osfs_open if needed
    .read = osfs_read,
    .write = osfs_write,
    .llseek = default_llseek,
    // Add other operations as needed
};

/**
 * Struct: osfs_file_inode_operations
 * Description: Defines the inode operations for regular files in osfs.
 * Note: Add additional operations such as getattr as needed.
 */
const struct inode_operations osfs_file_inode_operations = {
    // Add inode operations here, e.g., .getattr = osfs_getattr,
};
