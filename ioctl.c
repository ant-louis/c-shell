#include "fat.h"

#define FAT_IOCTL_SET_PROTECTED		_IOW('r', 0x14, __u32)
#define FAT_IOCTL_SET_UNPROTECTED	_IOW('r', 0x15, __u32)
#define FAT_IOCTL_SET_LOCK			_IOW('r', 0x16, __u32)
#define FAT_IOCTL_SET_UNLOCK		_IOW('r', 0x17, __u32)
#define FAT_IOCTL_SET_PASSWORD		_IOW('r', 0x18, __u32)


static int fat_ioctl_set_protected(struct file *file){
	printk("fat_ioctl_set_protected: entering");

	int err;
	u32 attr, oldattr;

	printk("fat_ioctl_set_protected: getting inode");
	struct inode *inode = file_inode(file);
	printk("fat_ioctl_set_protected: getting sbi");
	struct msdos_sb_info* sbi = MSDOS_SB(inode->i_sb);
	

	printk("fat_ioctl_set_protected: mnt_want_write_file");
	err = mnt_want_write_file(file);
	if (err)
		goto out;
	mutex_lock(&inode->i_mutex);
	printk("fat_ioctl_set_protected: in mutex");

	if(sbi->unlock == 0){ //If its locked, we don't do anything
		printk("fat_ioctl_set_protected: bit locked");
		goto out_unlock_inode;
	}

	oldattr = fat_make_attrs(inode);
	printk("fat_ioctl_set_protected: getting oldattr");

	attr = oldattr | ATTR_PROTECTED; /*We add ATTR_PROTECTED*/
	printk("fat_ioctl_set_protected: modifying attr");


	fat_save_attrs(inode, attr);
	printk("fat_ioctl_set_protected: saving attr");

	mark_inode_dirty(inode);
	printk("fat_ioctl_set_protected: marking inode dirty");

out_unlock_inode:
	mutex_unlock(&inode->i_mutex);
	mnt_drop_write_file(file);
	printk("fat_ioctl_set_protected: mnt_drop_write_file");

out:
	return err;
}


static int fat_ioctl_set_unprotected(struct file *file){
	
	printk("entering fat_ioctl_set_unprotected");

	int err;
	u32 attr, oldattr;
	
	printk("fat_ioctl_set_unprotected: getting inode");
	struct inode *inode = file_inode(file);
	printk("fat_ioctl_set_unprotected: getting sbi");
	struct msdos_sb_info* sbi = MSDOS_SB(inode->i_sb);
	
	printk("fat_ioctl_set_unprotected: in mutex");
	mutex_lock(&inode->i_mutex);

	printk("fat_ioctl_set_unprotected: mnt_want_write_file");
	err = mnt_want_write_file(file);
	if (err)
		goto out;
	
	if(sbi->unlock == 0){ //If its locked, we don't do anything
		printk("fat_ioctl_set_unprotected: bit locked");
		goto out_unlock_inode;
	}

	oldattr = fat_make_attrs(inode);
	printk("fat_ioctl_set_unprotected: getting oldattr");

	attr = oldattr & ~(ATTR_PROTECTED); 
	printk("fat_ioctl_set_unprotected: modifying attr");

	fat_save_attrs(inode, attr);
	printk("fat_ioctl_set_unprotected: saving attr");

	mark_inode_dirty(inode);
	printk("fat_ioctl_set_unprotected: marking inode dirty");

out_unlock_inode:
	mutex_unlock(&inode->i_mutex);
	mnt_drop_write_file(file);
	printk("fat_ioctl_set_unprotected: mnt_drop_write_file");

out:
	return err;

}


static int fat_ioctl_set_lock(struct file *file){

	struct inode *inode = file_inode(file);
	struct super_block *sb = inode->i_sb;
	struct msdos_sb_info *sbi = MSDOS_SB(sb);

	sbi->unlock = 0;

	return 0;
}



static int fat_ioctl_set_unlock(struct file *file, u32 __user *user_attr){

	printk("entering fat_ioctl_set_unlock");

	int err;
	u32 pw;
	struct fat_boot_sector *fbs;

	printk("fat_ioctl_set_unlock: getting inode");
	struct inode *inode = file_inode(file);
	struct super_block *sb = inode->i_sb;
	printk("fat_ioctl_set_unlock: getting sbi");
	struct msdos_sb_info* sbi = MSDOS_SB(sb);
	struct buffer_head *bh;

	printk("fat_ioctl_set_unlock: in mutex");
	mutex_lock(&inode->i_mutex);

	printk("fat_ioctl_set_unlock: get_user");
	err = get_user(pw, user_attr);
	if (err)
		goto out;
	
	bh = sb_bread(sb, 0);
	if (bh == NULL) {
		fat_msg(sb, KERN_ERR, "unable to read boot sector "
			"to mark fs as dirty");
		mutex_unlock(&inode->i_mutex);
		return -1;
	}
	
	fbs = (struct fat_boot_sector *) bh->b_data;

	//Check the password : ok if password corresponds or if no password set
	if((fbs->hidden == pw) || (fbs->hidden == 0 && pw == 0)){
		//The password is valid, so unlock everything
		sbi->unlock = 1;
		err = 0;
	}
	else{
		//The password is not valid
		err = 1;
	}

	printk("changepw: brelse");
	brelse(bh); //Clean buffer
	mutex_unlock(&inode->i_mutex);
	
out:
	return err;
}

static int fat_ioctl_set_password(struct file *file, u32 __user *user_attr){

	printk("entering fat_ioctl_set_password");

	int err;
	u32 attr;
	u16 curr_pw, new_pw, true_pw;
	struct fat_boot_sector *fbs;

	printk("fat_ioctl_set_password: getting inode");
	struct inode *inode = file_inode(file);
	printk("fat_ioctl_set_password: getting sb");
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh;
	
	printk("fat_ioctl_set_password: in mutex");
	mutex_lock(&inode->i_mutex);

	err = get_user(attr, user_attr);
	if (err)
		goto out;

	bh = sb_bread(sb,0);
	printk("fat_ioctl_set_password: sb_read");

	if (bh == NULL) {
		fat_msg(sb, KERN_ERR, "unable to read boot sector "
			"to mark fs as dirty");
		mutex_unlock(&inode->i_mutex);
		return -1;
	}

	fbs = (struct fat_boot_sector *) bh->b_data;
	printk("fat_ioctl_set_password: getting boot sector");

	curr_pw = attr & 0xff;
	new_pw = attr >> 16;
	true_pw = fbs->hidden;

	if(curr_pw == 0 && true_pw != 0){
		printk("fat_ioctl_set_password: old_pw null and hidden not 0");
		err = 1;

    }
    else if(curr_pw != 0 &&  curr_pw != true_pw){
		printk("fat_ioctl_set_password: curr_pw not null --> getting pw");
		err = 2; //The passord is not valid

	}
	else{
		printk("fat_ioctl_set_password: setting pw");
		fbs->hidden = new_pw;

		mark_buffer_dirty(bh); //Write it on disk
		printk("changepw: bf dirty");
		
		err = 0;
	}

	printk("changepw: brelse");
	brelse(bh);
	mutex_unlock(&inode->i_mutex);

out:
	return err;
}



long fat_generic_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	printk("fat_generic_ioctl: entering");

	struct inode *inode = file_inode(file);
	u32 __user *user_attr = (u32 __user *)arg;
	printk("fat_generic_ioctl: getting inode");

	switch (cmd) {
	case FAT_IOCTL_GET_ATTRIBUTES:
		return fat_ioctl_get_attributes(inode, user_attr);
	case FAT_IOCTL_SET_ATTRIBUTES:
		return fat_ioctl_set_attributes(file, user_attr);
	case FAT_IOCTL_GET_VOLUME_ID:
		return fat_ioctl_get_volume_id(inode, user_attr);
	case FAT_IOCTL_SET_PROTECTED:
		printk("fat_generic_ioctl: SET PROTECTED CASE");
		return fat_ioctl_set_protected(file);
	case FAT_IOCTL_SET_UNPROTECTED:
		printk("fat_generic_ioctl: SET UNPROTECTED CASE");
		return fat_ioctl_set_unprotected(file);
	case FAT_IOCTL_SET_LOCK:
		printk("fat_generic_ioctl: FAT_IOCTL_SET_LOCK");
		return fat_ioctl_set_lock(file, user_attr);
	case FAT_IOCTL_SET_UNLOCK:
		printk("fat_generic_ioctl: FAT_IOCTL_SET_UNLOCK");
		return fat_ioctl_set_unlock(file, user_attr);
	case FAT_IOCTL_SET_PASSWORD:
		printk("fat_generic_ioctl: FAT_IOCTL_SET_PASSWORD");
		return fat_ioctl_set_password(file, user_attr);
	default:
		return -ENOTTY;	/* Inappropriate ioctl for device */
	}
}