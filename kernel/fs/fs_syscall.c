/* This file use for NCTU OSDI course */

// It's handel the file system APIs 
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <fs.h>
#include <kernel/mem.h>
#include <kernel/task.h>

/*TODO: Lab7, file I/O system call interface.*/
/*NOte: Here you need handle the file system call from user.
*       1. When user open a new file, you can use the fd_new() to alloc a file object(struct fs_fd)
*       2. When user R/W the file, use the fd_get() to get file object.
*       3. After get file object call file_* functions into VFS level
*       4. Update the file objet's position or size when user R/W or seek the file.(You can find the useful marco in ff.h)
*       5. Handle the error code, for example, if user call open() but no fd slot can be use, sys_open should return -STATUS_ENOSPC.
*/
extern struct fs_fd fd_table[];
extern struct fs_dev fat_fs;
extern Task *cur_task;

// Below is POSIX like I/O system call 
int sys_open(const char *file, int flags, int mode)
{
  //We dont care the mode.
	/* lab7 */
	int i, retVal = -1;
	int findFlag = false;
	struct fs_fd *find_fd = NULL;

	for(i = 0; i < FS_FD_MAX; i++){
		find_fd = &fd_table[i];
		if(!strcmp(find_fd->path, file)){
			findFlag = true;
			break;
		}
	}

	if(findFlag == false){
		for(i = 0; i < FS_FD_MAX; i++){
			find_fd = &fd_table[i];
			if(!strcmp(fd_table[i].path, "")){
				strcpy(find_fd->path, file);
				findFlag = true;
				break;
			}
		}

	}

	if(findFlag == true){
		find_fd->flags = flags;
		retVal = find_fd->fs->ops->open(find_fd);
		if(find_fd->ref_count > 9)
			return -1;
		else
			find_fd->ref_count++;
	}
	else
		return -1;

	if(find_fd->flags & O_APPEND){
		int tmpRetVal = sys_lseek(find_fd, 0, SEEK_END);
		printk("append tmpRetVal = %d, pos = %d, size = %d\n", tmpRetVal, find_fd->pos, 
					 find_fd->size);
	}

	printk("Open file: %s id: %d\n", file, i);
	return i;
}

int sys_close(int fd)
{
	/* lab7 */
	int retVal;
	struct fs_fd *p_fd = &fd_table[fd];

	retVal = p_fd->fs->ops->close(p_fd);

	if(retVal == 0)
		if(p_fd->ref_count > 0)
			p_fd->ref_count--;

	printk("sys_close with retVal: %d\n", retVal);
	return retVal;
}

int sys_read(int fd, void *buf, size_t len)
{
	/* lab7 */
	int retVal;
	int count;
	struct fs_fd *p_fd = &fd_table[fd];

	struct PageInfo* page = page_lookup(cur_task->pgdir, buf, NULL);
	if(page == NULL)
		return -STATUS_EINVAL;

	printk("[sys_read] Start from %d, len = %d\n", p_fd->pos, p_fd->size);	
	if(p_fd->pos + len > p_fd->size)
		count = p_fd->size - p_fd->pos;
	else
		count = len;

	retVal = p_fd->fs->ops->read(p_fd, buf, count);

	printk("sys_read with retVal: %d\n", retVal);
	return retVal;
}

int sys_write(int fd, const void *buf, size_t len)
{
	/* lab7 */
	int retVal;
	struct fs_fd *p_fd = &fd_table[fd];

	struct PageInfo* page = page_lookup(cur_task->pgdir, buf, NULL);
	if(page == NULL)
		return -STATUS_EINVAL;

	printk("[sys_write] Start from %d, len = %d\n", p_fd->pos, p_fd->size);	
	retVal = p_fd->fs->ops->write(p_fd, buf, len);

	//printk("Write file with retVal: %d\n", retVal);
	return retVal;
}

/* Note: Check the whence parameter and calcuate the new offset value before do file_seek() */
off_t sys_lseek(int fd, off_t offset, int whence)
{
	/* lab7 */
	int retVal;
	struct fs_fd *p_fd = &fd_table[fd];
	off_t new_offset = 0;

	switch(whence){
		case SEEK_END:
			new_offset = p_fd->size + offset;
			break;
		case SEEK_SET:
			new_offset = offset;
			break;
	}

	retVal = p_fd->fs->ops->lseek(p_fd, new_offset);
	if(retVal == 0){
		p_fd->pos = new_offset;
		return new_offset;
	}

	return retVal;
}

#include <fat/ff.h>
int sys_unlink(const char *pathname)
{
	/* lab7 */ 
	int i, retVal = -1, findFlag = false;
	struct fs_fd *find_fd;

	
	for(i = 0; i < FS_FD_MAX; i++){
		find_fd = &fd_table[i];
		if(!strcmp(find_fd->path, pathname)){
			findFlag = true;
			retVal = find_fd->fs->ops->unlink(find_fd, *pathname);
			if(retVal == 0){
				strcpy(find_fd->path, "");
				find_fd->ref_count = 0;
			}
			break;
		}
	}

	if(findFlag == false)
		printk("sys_unlink: Can not find file %s\n", pathname);
	else
		printk("sys_unlink unlink file %s with retVal %d\n", pathname, retVal);

	return retVal;
}

int sys_list(const char *pathname)
{
	DIR dir;
	FILINFO fno;
	int res;

	printk("ls folder = %s\n", pathname);
	f_opendir(&dir, pathname);
	res = f_readdir(&dir, &fno);
	while (strlen(fno.fname)) {
		printk("[%s] ret = %d, filename = %s, fsize = %d, date = %d, time = %d\n",
					  __func__, res, fno.fname, fno.fsize, fno.fdate, fno.ftime);
		res = f_readdir(&dir, &fno);
	}
	f_closedir(&dir);

	return 0;
}
