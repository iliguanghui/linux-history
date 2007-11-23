/*
 *  linux/fs/ext2/symlink.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/symlink.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  ext2 symlink handling code
 */

#include <asm/uaccess.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/stat.h>

static int ext2_readlink (struct inode *, char *, int);
static int ext2_follow_link (struct inode *, struct inode *, int, int,
			       struct inode **);

/*
 * symlinks can't do much...
 */
struct inode_operations ext2_symlink_inode_operations = {
	NULL,			/* no file-operations */
	NULL,			/* create */
	NULL,			/* lookup */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* rmdir */
	NULL,			/* mknod */
	NULL,			/* rename */
	ext2_readlink,		/* readlink */
	ext2_follow_link,	/* follow_link */
	NULL,			/* readpage */
	NULL,			/* writepage */
	NULL,			/* bmap */
	NULL,			/* truncate */
	NULL,			/* permission */
	NULL			/* smap */
};

static int ext2_follow_link(struct inode * dir, struct inode * inode,
			    int flag, int mode, struct inode ** res_inode)
{
	int error;
	struct buffer_head * bh = NULL;
	char * link;

	*res_inode = NULL;
	if (!dir) {
		dir = current->fs->root;
		dir->i_count++;
	}
	if (!inode) {
		iput (dir);
		return -ENOENT;
	}
	if (!S_ISLNK(inode->i_mode)) {
		iput (dir);
		*res_inode = inode;
		return 0;
	}
	if (current->link_count > 5) {
		iput (dir);
		iput (inode);
		return -ELOOP;
	}
	if (inode->i_blocks) {
		if (!(bh = ext2_bread (inode, 0, 0, &error))) {
			iput (dir);
			iput (inode);
			return -EIO;
		}
		link = bh->b_data;
	} else
		link = (char *) inode->u.ext2_i.i_data;
	if (DO_UPDATE_ATIME(inode)) {
		inode->i_atime = CURRENT_TIME;
		inode->i_dirt = 1;
	}
	current->link_count++;
	error = open_namei (link, flag, mode, res_inode, dir);
	current->link_count--;
	iput (inode);
	if (bh)
		brelse (bh);
	return error;
}

static int ext2_readlink (struct inode * inode, char * buffer, int buflen)
{
	struct buffer_head * bh = NULL;
	char * link;
	int i, err;

	if (!S_ISLNK(inode->i_mode)) {
		iput (inode);
		return -EINVAL;
	}
	if (buflen > inode->i_sb->s_blocksize - 1)
		buflen = inode->i_sb->s_blocksize - 1;
	if (inode->i_blocks) {
		bh = ext2_bread (inode, 0, 0, &err);
		if (!bh) {
			iput (inode);
			return 0;
		}
		link = bh->b_data;
	}
	else
		link = (char *) inode->u.ext2_i.i_data;
	
	/* XXX I hope link is always '\0'-terminated. */ 	
	i = strlen(link);
	if (i >= buflen)
		i = buflen-1;
	if (copy_to_user(buffer, link, i+1))
		i = -EFAULT;
 	if (DO_UPDATE_ATIME(inode)) {
		inode->i_atime = CURRENT_TIME;
		inode->i_dirt = 1;
	}
	iput (inode);
	if (bh)
		brelse (bh);
	return i;
}
