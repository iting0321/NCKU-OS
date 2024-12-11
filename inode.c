#include <linux/fs.h>
#include <linux/uaccess.h>
#include "osfs.h"

/**
 * Function: osfs_get_osfs_inode
 * Description: Retrieves the osfs_inode structure for a given inode number.
 * Inputs:
 *   - sb: The superblock of the filesystem.
 *   - ino: The inode number to retrieve.
 * Returns:
 *   - A pointer to the osfs_inode structure if successful.
 *   - NULL if the inode number is invalid or out of bounds.
 */
struct osfs_inode *osfs_get_osfs_inode(struct super_block *sb, uint32_t ino)
{
    struct osfs_sb_info *sb_info = sb->s_fs_info;

    if (ino == 0 || ino >= sb_info->inode_count) // File system inode count upper bound
        return NULL;
    return &((struct osfs_inode *)(sb_info->inode_table))[ino];
}

/**
 * Function: osfs_get_free_inode
 * Description: Allocates a free inode number from the inode bitmap.
 * Inputs:
 *   - sb_info: The superblock information of the filesystem.
 * Returns:
 *   - The allocated inode number on success.
 *   - -ENOSPC if no free inode is available.
 */
int osfs_get_free_inode(struct osfs_sb_info *sb_info)
{
    uint32_t ino;

    for (ino = 1; ino < sb_info->inode_count; ino++) {
        if (!test_bit(ino, sb_info->inode_bitmap)) {
            set_bit(ino, sb_info->inode_bitmap);
            sb_info->nr_free_inodes--;
            return ino;
        }
    }
    pr_err("osfs_get_free_inode: No free inode available\n");
    return -ENOSPC;
}
void osfs_free_extents(struct osfs_inode *osfs_inode)
{
    struct osfs_extent *cur = osfs_inode->extent_list;
    struct osfs_extent *next;

    while (cur) {
        next = cur->next;   // Save the pointer to the next extent
        struct osfs_extent *tmp = cur;
        kfree(tmp);         // Free the cur extent
        cur = next;         // Move to the next extent
    }

    osfs_inode->extent_list = NULL;
    osfs_inode->i_blocks = 0;
}

/**
 * Function: is_block_range_free
 * Description: Checks if a range of blocks is free in the block bitmap.
 * Inputs:
 *   - sb_info: The superblock information (contains the block bitmap).
 *   - start_block: The starting block of the range to check.
 *   - length: The number of blocks to check.
 * Returns:
 *   - true (1) if all blocks in the range are free.
 *   - false (0) if any block in the range is occupied.
 */
int is_block_range_free(struct osfs_sb_info *sb_info, uint32_t start_block, uint32_t length)
{
    uint32_t i;

    for (i = start_block; i < start_block + length; i++) {
        // If any block is occupied, return false (0)
        if (test_bit(i, sb_info->block_bitmap)) {
            return 0; // Block is in use
        }
    }

    return 1; // All blocks in the range are free
}
uint32_t osfs_find_free_blocks(struct osfs_sb_info *sb_info, uint32_t length) {
    for (uint32_t i = 0; i < sb_info->data_blocks - length; i++) {
        if (is_block_range_free(sb_info, i, length))
            return i;
    }
    return INVALID_BLOCK;
}
/**
 * Function: osfs_iget
 * Description: Creates or retrieves a VFS inode from a given inode number.
 * Inputs:
 *   - sb: The superblock of the filesystem.
 *   - ino: The inode number to load.
 * Returns:
 *   - A pointer to the VFS inode on success.
 *   - ERR_PTR(-EFAULT) if the osfs_inode cannot be retrieved.
 *   - ERR_PTR(-ENOMEM) if memory allocation for the inode fails.
 */
struct inode *osfs_iget(struct super_block *sb, unsigned long ino)
{
    struct osfs_inode *osfs_inode;
    struct inode *inode;

    osfs_inode = osfs_get_osfs_inode(sb, ino);
    if (!osfs_inode)
        return ERR_PTR(-EFAULT);

    inode = new_inode(sb);
    if (!inode)
        return ERR_PTR(-ENOMEM);

    inode->i_ino = ino;
    inode->i_sb = sb;
    inode->i_mode = osfs_inode->i_mode;
    i_uid_write(inode, osfs_inode->i_uid);
    i_gid_write(inode, osfs_inode->i_gid);
    inode->__i_atime = osfs_inode->__i_atime;
    inode->__i_mtime = osfs_inode->__i_mtime;
    inode->__i_ctime = osfs_inode->__i_ctime;
    inode->i_size = osfs_inode->i_size;
    inode->i_blocks = osfs_inode->i_blocks;
    inode->i_private = osfs_inode;

    if (S_ISDIR(inode->i_mode)) {
        inode->i_op = &osfs_dir_inode_operations;
        inode->i_fop = &osfs_dir_operations;
    } else if (S_ISREG(inode->i_mode)) {
        inode->i_op = &osfs_file_inode_operations;
        inode->i_fop = &osfs_file_operations;
    }

    // Insert the inode into the inode hash
    insert_inode_hash(inode);

    return inode;
}

/**
 * Function: osfs_alloc_data_block
 * Description: Allocates a free data block from the block bitmap.
 * Inputs:
 *   - sb_info: The superblock information of the filesystem.
 *   - block_no: Pointer to store the allocated block number.
 * Returns:
 *   - 0 on successful allocation.
 *   - -ENOSPC if no free data block is available.
 */
int osfs_alloc_data_block(struct osfs_sb_info *sb_info, uint32_t *block_no)
{
    uint32_t i;

    for (i = 0; i < sb_info->block_count; i++) {
        if (!test_bit(i, sb_info->block_bitmap)) {
            set_bit(i, sb_info->block_bitmap);
            sb_info->nr_free_blocks--;
            *block_no = i;
            return 0;
        }
    }
    pr_err("osfs_alloc_data_block: No free data block available\n");
    return -ENOSPC;
}
int osfs_alloc_extent(struct osfs_sb_info *sb_info, uint32_t *start_block, uint32_t length) {
    uint32_t free_block = osfs_find_free_blocks(sb_info, length); // Find a range of free blocks
    if (free_block == INVALID_BLOCK)
        return -ENOSPC; // No space available

    osfs_mark_blocks_used(sb_info, free_block, length); // Mark the blocks as used
    *start_block = free_block;
    return 0;
}
int osfs_add_extent(struct osfs_inode *osfs_inode, uint32_t start_block, uint32_t length)
{
    struct osfs_extent *new_extent, *cur;

    // Allocate memory for the new extent
    new_extent = malloc(sizeof(struct osfs_extent), GFP_KERNEL);
    if (!new_extent)
        return -ENOMEM;

    new_extent->start_block = start_block;
    new_extent->length = length;
    new_extent->next = NULL;

    // Append to the linked list
    if (!osfs_inode->extent_list) {
        osfs_inode->extent_list = new_extent; // First extent
    } else {
        cur = osfs_inode->extent_list;
        while (cur->next) {
            cur = cur->next; // Traverse to the end
        }
        cur->next = new_extent; // Append at the end
    }

    osfs_inode->i_blocks += length; // Update total block count
    return 0;
}

void osfs_free_data_block(struct osfs_sb_info *sb_info, uint32_t block_no)
{
    clear_bit(block_no, sb_info->block_bitmap);
    sb_info->nr_free_blocks++;
}
void set_block_bitmap(struct osfs_sb_info *sb_info, uint32_t block_no)
{
    set_bit(block_no, sb_info->block_bitmap);  // Set the bit corresponding to the block number in the block bitmap
}

void osfs_mark_blocks_used(struct osfs_sb_info *sb_info, uint32_t start_block, uint32_t length) {
    for (uint32_t i = start_block; i < start_block + length; i++) {
        set_block_bitmap(sb_info, i);
    }
}

