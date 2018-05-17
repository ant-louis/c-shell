#include "fat.h"

#define FAT_IOCTL_SET_PROTECTED		_IOW('r', 0x14, __u32)
#define FAT_IOCTL_SET_UNPROTECTED	_IOW('r', 0x15, __u32)
#define FAT_IOCTL_SET_LOCK			_IOW('r', 0x16, __u32)
#define FAT_IOCTL_SET_UNLOCK		_IOW('r', 0x17, __u32)
#define FAT_IOCTL_SET_PASSWORD		_IOW('r', 0x18, __u32)


static int fat_ioctl_set_protected(struct file *file){

	int err;
	u32 attr, oldattr;
	struct inode *inode = file_inode(file);
	struct msdos_sb_info* sbi = MSDOS_SB(inode->i_sb);
	

	err = mnt_want_write_file(file);
	if (err)
		goto out;

	mutex_lock(&inode->i_mutex);
	
	//If its locked, we don't do anything
	if(sbi->unlock == 0)
		goto out_unlock_inode;

	//Otherwise
	oldattr = fat_make_attrs(inode);
	attr = oldattr | ATTR_PROTECTED; /*We add ATTR_PROTECTED*/

	fat_save_attrs(inode, attr);
	mark_inode_dirty_sync(inode);

out_unlock_inode:
	mutex_unlock(&inode->i_mutex);
	mnt_drop_write_file(file);

out:
	return err;
}


static int fat_ioctl_set_unprotected(struct file *file){
	
	int err;
	u32 attr, oldattr;
	struct inode *inode = file_inode(file);
	struct msdos_sb_info* sbi = MSDOS_SB(inode->i_sb);
	

	err = mnt_want_write_file(file);
	if (err)
		goto out;

	mutex_lock(&inode->i_mutex);
	
	//If its locked, we don't do anything
	if(sbi->unlock == 0)
		goto out_unlock_inode;

	//Otherwise
	oldattr = fat_make_attrs(inode);
	attr = oldattr & ~(ATTR_PROTECTED); 

	fat_save_attrs(inode, attr);
	mark_inode_dirty_sync(inode);

out_unlock_inode:
	mutex_unlock(&inode->i_mutex);
	mnt_drop_write_file(file);

out:
	return err;
}


static int fat_ioctl_set_lock(struct inode *inode){

	struct super_block *sb = inode->i_sb;
	struct msdos_sb_info *sbi = MSDOS_SB(sb);
	
	sbi->unlock = 0;

	return 0;
}


static int fat_ioctl_set_unlock(struct inode *inode, u32 __user *user_attr){

	int err;
	u32 pw;
	struct fat_boot_sector *fbs;
	struct buffer_head *bh;
	struct super_block *sb = inode->i_sb;
	struct msdos_sb_info* sbi = MSDOS_SB(sb);


	err = get_user(pw, user_attr);
	if (err)
		goto out;
	
	mutex_lock(&inode->i_mutex);
	
	bh = sb_bread(sb, 0);
	if (bh == NULL) {
		fat_msg(sb, KERN_ERR, "unable to read boot sector "
			"to mark fs as dirty");
		mutex_unlock(&inode->i_mutex);
		return -1;
	}
	
	fbs = (struct fat_boot_sector *) bh->b_data;
	

	//Check the password : ok if password corresponds or if no password set
	if(fbs->hidden == pw){
		//The password is valid, so unlock everything
		sbi->unlock = 1;
		err = 0;
	}
	else{
		//The password is not valid
		err = 1;
	}

	brelse(bh); //Clean buffer
	mutex_unlock(&inode->i_mutex);
	
out:
	return err;
}

static int fat_ioctl_set_password(struct inode *inode, u32 __user *user_attr){

	int err;
	u32 attr;
	u16 given_pw, new_pw, true_pw;
	struct fat_boot_sector *fbs;
	struct buffer_head *bh;
	struct super_block *sb = inode->i_sb;

	
	err = get_user(attr, user_attr);
	if (err)
		goto out;

	mutex_lock(&inode->i_mutex);

	bh = sb_bread(sb,0);
	if (bh == NULL) {
		fat_msg(sb, KERN_ERR, "unable to read boot sector "
			"to mark fs as dirty");
		mutex_unlock(&inode->i_mutex);
		return -1;
	}

	fbs = (struct fat_boot_sector *) bh->b_data;

	//Dividing the given attribute to the different passwords
	given_pw = attr & 0xffff;
	new_pw = attr >> 16;
	true_pw = fbs->hidden;

	//If no password given but a password already exists -> error
	if(given_pw == 0 && true_pw != 0)
		err = 1;

    //If a password is given but doesn't correspond to the current one -> error
    else if(given_pw != 0 &&  given_pw != true_pw)
		err = 2;

	//All other cases (no password given and no password set - password given ok)
	else{
		fbs->hidden = new_pw;
		mark_buffer_dirty(bh); //Write it on disk		
		err = 0;
	}

	brelse(bh);
	mutex_unlock(&inode->i_mutex);

out:
	return err;
}



long fat_generic_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	struct inode *inode = file_inode(filp);
	u32 __user *user_attr = (u32 __user *)arg;

	switch (cmd) {
	case FAT_IOCTL_GET_ATTRIBUTES:
		return fat_ioctl_get_attributes(inode, user_attr);
	case FAT_IOCTL_SET_ATTRIBUTES:
		return fat_ioctl_set_attributes(filp, user_attr);
	case FAT_IOCTL_GET_VOLUME_ID:
		return fat_ioctl_get_volume_id(inode, user_attr);
	case FAT_IOCTL_SET_PROTECTED:
		return fat_ioctl_set_protected(filp);
	case FAT_IOCTL_SET_UNPROTECTED:
		return fat_ioctl_set_unprotected(filp);
	case FAT_IOCTL_SET_LOCK:
		return fat_ioctl_set_lock(inode);
	case FAT_IOCTL_SET_UNLOCK:
		return fat_ioctl_set_unlock(inode, user_attr);
	case FAT_IOCTL_SET_PASSWORD:
		return fat_ioctl_set_password(inode, user_attr);
	default:
		return -ENOTTY;	/* Inappropriate ioctl for device */
	}
}


static int fat_file_open(struct inode *inode, struct file *filp){

	struct super_block *sb;
	struct msdos_sb_info *sbi;
	int err;
	
	mutex_lock(&inode->i_mutex);

	sb = inode->i_sb;
	sbi = MSDOS_SB(sb);

	if((sbi->unlock == 0) && (MSDOS_I(inode)->i_attrs & ATTR_PROTECTED)){
		err = -ENOENT;
		goto out;
	}
	err = 0;

out_unlock_inode:
	mutex_lock(&inode->i_mutex);

out:
	return err;
}