#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "osfs.h"

/**
 * Function: osfs_lookup
 * Description: Looks up a file within a directory.
 * Inputs:
 *   - dir: The inode of the directory to search in.
 *   - dentry: The dentry representing the file to look up.
 *   - flags: Flags for the lookup operation.
 * Returns:
 *   - A pointer to the dentry if the file is found.
 *   - NULL if the file is not found, allowing the VFS to handle it.
 */
static struct dentry *osfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
    struct osfs_sb_info *sb_info = dir->i_sb->s_fs_info;
    struct osfs_inode *parent_inode = dir->i_private;
    struct osfs_dir_entry *dir_entries;
    struct inode *inode = NULL;
    void *dir_data_block;
    int i;
    struct osfs_extent *extent;

    pr_info("osfs_lookup: Looking up '%.*s' in inode %lu\n",
            (int)dentry->d_name.len, dentry->d_name.name, dir->i_ino);

    // Traverse all extents of the directory (using linked list)
    extent = parent_inode->extent_list;  // Start from the first extent in the linked list
    while (extent) {
        dir_data_block = sb_info->data_blocks + extent->start_block * BLOCK_SIZE;
        int dir_entry_count = extent->length * BLOCK_SIZE / sizeof(struct osfs_dir_entry);
        dir_entries = (struct osfs_dir_entry *)dir_data_block;

        // Search for the file within this extent
        for (i = 0; i < dir_entry_count; i++) {
            if (strlen(dir_entries[i].filename) == dentry->d_name.len &&
                strncmp(dir_entries[i].filename, dentry->d_name.name, dentry->d_name.len) == 0) {
                // File found, get inode
                inode = osfs_iget(dir->i_sb, dir_entries[i].inode_no);
                if (IS_ERR(inode)) {
                    pr_err("osfs_lookup: Error getting inode %u\n", dir_entries[i].inode_no);
                    return ERR_CAST(inode);
                }
                return d_splice_alias(inode, dentry);
            }
        }

        extent = extent->next; // Move to the next extent in the linked list
    }

    pr_info("osfs_lookup: File '%.*s' not found\n", (int)dentry->d_name.len, dentry->d_name.name);
    return NULL; // File not found
}


/**
 * Function: osfs_iterate
 * Description: Iterates over the entries in a directory.
 * Inputs:
 *   - filp: The file pointer representing the directory.
 *   - ctx: The directory context used for iteration.
 * Returns:
 *   - 0 on successful iteration.
 *   - A negative error code on failure.
 */
static int osfs_iterate(struct file *filp, struct dir_context *ctx)
{
    struct inode *inode = file_inode(filp);
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_extent *extent = osfs_inode->extent_list;
    struct osfs_dir_entry *dir_entries;
    void *dir_data_block;
    int dir_entry_count, i;
    size_t processed_entries = 0;

    // Emit '.' and '..' entries at the beginning
    if (ctx->pos == 0) {
        if (!dir_emit_dots(filp, ctx)) {
            pr_warn("osfs_iterate: Failed to emit '.' and '..'\n");
            return 0; // No more entries to iterate
        }
    }

    // Limit the iteration based on the directory size
    while (extent && processed_entries < (osfs_inode->i_size / sizeof(struct osfs_dir_entry))) {
        dir_data_block = sb_info->data_blocks + extent->start_block * BLOCK_SIZE;
        dir_entries = (struct osfs_dir_entry *)dir_data_block;
        dir_entry_count = (extent->length * BLOCK_SIZE) / sizeof(struct osfs_dir_entry);

        for (i = 0; i < dir_entry_count; i++) {
            struct osfs_dir_entry *entry = &dir_entries[i];

            // Debugging: Print the current entry being processed
            pr_debug("osfs_iterate: Entry[%d] filename='%s', inode_no=%u\n", i, entry->filename, entry->inode_no);

            // Stop if we've processed all valid entries
            if (processed_entries >= (osfs_inode->i_size / sizeof(struct osfs_dir_entry))) {
                return 0;
            }

            // Skip invalid or empty entries
            if (strlen(entry->filename) == 0 || entry->inode_no == 0) {
                pr_warn("osfs_iterate: Skipping invalid entry (filename='%s', inode_no=%u)\n",
                        entry->filename, entry->inode_no);
                continue;
            }

            // Emit the directory entry
            if (!dir_emit(ctx, entry->filename, strlen(entry->filename), entry->inode_no, DT_UNKNOWN)) {
                pr_warn("osfs_iterate: Buffer full, stopping directory iteration\n");
                return 0; // Stop iteration gracefully if buffer is full
            }

            // Update directory position and processed entry count
            ctx->pos++;
            processed_entries++;
        }

        extent = extent->next; // Move to the next extent
    }

    return 0;
}



/**
 * Function: osfs_new_inode
 * Description: Creates a new inode within the filesystem.
 * Inputs:
 *   - dir: The inode of the directory where the new inode will be created.
 *   - mode: The mode (permissions and type) for the new inode.
 * Returns:
 *   - A pointer to the newly created inode on success.
 *   - ERR_PTR(-EINVAL) if the file type is not supported.
 *   - ERR_PTR(-ENOSPC) if there are no free inodes or blocks.
 *   - ERR_PTR(-ENOMEM) if memory allocation fails.
 *   - ERR_PTR(-EIO) if an I/O error occurs.
 */
struct inode *osfs_new_inode(const struct inode *dir, umode_t mode)
{
    struct super_block *sb = dir->i_sb;
    struct osfs_sb_info *sb_info = sb->s_fs_info;
    struct inode *inode;
    struct osfs_inode *osfs_inode;
    int ino, ret;

    // Validate mode
    if (!S_ISDIR(mode) && !S_ISREG(mode) && !S_ISLNK(mode)) {
        pr_err("osfs_new_inode: Unsupported file type\n");
        return ERR_PTR(-EINVAL);
    }

    // Ensure free inodes and blocks are available
    if (sb_info->nr_free_inodes == 0 || sb_info->nr_free_blocks == 0)
        return ERR_PTR(-ENOSPC);

    // Allocate inode number
    ino = osfs_get_free_inode(sb_info);
    if (ino < 0 || ino >= sb_info->inode_count)
        return ERR_PTR(-ENOSPC);

    // Allocate VFS inode
    inode = new_inode(sb);
    if (!inode)
        return ERR_PTR(-ENOMEM);

    // Initialize inode
    inode_init_owner(&nop_mnt_idmap, inode, dir, mode);
    inode->i_ino = ino;
    inode->i_sb = sb;
    inode->i_blocks = 0;
    simple_inode_init_ts(inode);

    // Set inode operations
    if (S_ISDIR(mode)) {
        inode->i_op = &osfs_dir_inode_operations;
        inode->i_fop = &osfs_dir_operations;
        set_nlink(inode, 2); // . and ..
    } else if (S_ISREG(mode)) {
        inode->i_op = &osfs_file_inode_operations;
        inode->i_fop = &osfs_file_operations;
        set_nlink(inode, 1);
    }

    // Get osfs_inode
    osfs_inode = osfs_get_osfs_inode(sb, ino);
    if (!osfs_inode) {
        pr_err("osfs_new_inode: Failed to get osfs_inode\n");
        iput(inode);
        return ERR_PTR(-EIO);
    }
    memset(osfs_inode, 0, sizeof(*osfs_inode));

    // Allocate initial extent for the inode (linked list)
    uint32_t start_block;
    ret = osfs_alloc_extent(sb_info, &start_block, 1);  // Allocate 1 block for the first extent
    if (ret) {
        pr_err("osfs_new_inode: Failed to allocate extent\n");
        iput(inode);
        return ERR_PTR(ret);
    }

    // Create and add the first extent to the linked list
    struct osfs_extent *new_extent = kmalloc(sizeof(struct osfs_extent), GFP_KERNEL);
    if (!new_extent) {
        pr_err("osfs_new_inode: Memory allocation for new extent failed\n");
        iput(inode);
        return ERR_PTR(-ENOMEM);
    }

    new_extent->start_block = start_block;
    new_extent->length = 1;  // 1 block in this extent
    new_extent->next = NULL; // No next extent yet
    
    // Add this new extent to the linked list
    osfs_inode->extent_list = new_extent;

    // Update inode and osfs_inode attributes
    osfs_inode->num_extents = 1;
    osfs_inode->i_ino = ino;
    osfs_inode->i_mode = mode;
    osfs_inode->i_size = 0;  // Initially the file size is 0
    osfs_inode->i_blocks = 1;  // One block allocated
    osfs_inode->__i_atime = osfs_inode->__i_mtime = osfs_inode->__i_ctime = current_time(inode);
    inode->i_private = osfs_inode;

    // Update superblock
    sb_info->nr_free_inodes--;

    return inode;
}

int osfs_add_dir_entry(struct inode *dir, uint32_t inode_no, const char *name, size_t name_len)
{
    struct osfs_sb_info *sb_info = dir->i_sb->s_fs_info;
    struct osfs_inode *parent_inode = dir->i_private;
    struct osfs_dir_entry *dir_entries;
    void *dir_data_block;
    struct osfs_extent *extent;
    int dir_entry_count, i, ret;

    // Traverse the extents of the directory
    extent = parent_inode->extent_list;
    while (extent) {
        dir_data_block = sb_info->data_blocks + extent->start_block * BLOCK_SIZE;
        dir_entries = (struct osfs_dir_entry *)dir_data_block;
        dir_entry_count = (extent->length * BLOCK_SIZE) / sizeof(struct osfs_dir_entry);

        // Search for an empty slot in the current extent
        for (i = 0; i < dir_entry_count; i++) {
            if (dir_entries[i].inode_no == 0) {  // Empty slot found
                memset(dir_entries[i].filename, 0, MAX_FILENAME_LEN); // Clear the filename field
                strncpy(dir_entries[i].filename, name, name_len);
                dir_entries[i].filename[name_len] = '\0';
                dir_entries[i].inode_no = inode_no;
                parent_inode->i_size += sizeof(struct osfs_dir_entry);
                return 0;  // Successfully added the entry
            }
        }

        extent = extent->next; // Move to the next extent
    }

    // No free slots found, allocate a new extent
    uint32_t new_start_block;
    ret = osfs_alloc_extent(sb_info, &new_start_block, 1); // Allocate 1 block for the new extent
    if (ret) {
        pr_err("osfs_add_dir_entry: Failed to allocate new extent\n");
        return ret;
    }

    // Add the new extent
    osfs_add_extent(parent_inode, new_start_block, 1);

    // Initialize the new extent's data
    dir_data_block = sb_info->data_blocks + new_start_block * BLOCK_SIZE;
    dir_entries = (struct osfs_dir_entry *)dir_data_block;
    memset(dir_entries, 0, BLOCK_SIZE);  // Clear the newly allocated block
    strncpy(dir_entries[0].filename, name, name_len);
    dir_entries[0].filename[name_len] = '\0';
    dir_entries[0].inode_no = inode_no;
    parent_inode->i_size += sizeof(struct osfs_dir_entry);

    return 0;
}


/**
 * Function: osfs_create
 * Description: Creates a new file within a directory.
 * Inputs:
 *   - idmap: The mount namespace ID map.
 *   - dir: The inode of the parent directory.
 *   - dentry: The dentry representing the new file.
 *   - mode: The mode (permissions and type) for the new file.
 *   - excl: Whether the creation should be exclusive.
 * Returns:
 *   - 0 on successful creation.
 *   - -EEXIST if the file already exists.
 *   - -ENAMETOOLONG if the file name is too long.
 *   - -ENOSPC if the parent directory is full.
 *   - A negative error code from osfs_new_inode on failure.
 */

static int osfs_create(struct mnt_idmap *idmap, struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    struct osfs_inode *parent_inode = dir->i_private;  // Parent directory inode
    struct inode *inode;
    int ret;

    // Step 1: Validate the filename length
    if (dentry->d_name.len > MAX_FILENAME_LEN) {
        pr_err("osfs_create: File name too long\n");
        return -ENAMETOOLONG;
    }

    // Step 2: Create a new inode for the file
    inode = osfs_new_inode(dir, mode);
    if (IS_ERR(inode)) {
        pr_err("osfs_create: Failed to create new inode\n");
        return PTR_ERR(inode);
    }

    // Step 3: Add the new file to the parent directory
    ret = osfs_add_dir_entry(dir, inode->i_ino, dentry->d_name.name, dentry->d_name.len);
    if (ret) {
        pr_err("osfs_create: Failed to add directory entry\n");
        iput(inode);  // Clean up inode if directory entry creation fails
        return ret;
    }

    // Step 4: Update parent directory metadata
    parent_inode->i_size += sizeof(struct osfs_dir_entry);
    parent_inode->__i_mtime = current_time(dir);  // Update modification time
    mark_inode_dirty(dir);  // Mark the directory inode as dirty to persist changes

    // Step 5: Link the newly created inode to the VFS dentry
    d_instantiate(dentry, inode);

    pr_info("osfs_create: File '%.*s' created with inode %lu\n",
            (int)dentry->d_name.len, dentry->d_name.name, inode->i_ino);

    return 0;
}





const struct inode_operations osfs_dir_inode_operations = {
    .lookup = osfs_lookup,
    .create = osfs_create,
    // Add other operations as needed
};

const struct file_operations osfs_dir_operations = {
    .iterate_shared = osfs_iterate,
    .llseek = generic_file_llseek,
    // Add other operations as needed
};
