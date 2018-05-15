SYSCALL_DEFINE2(pfstat, pid_t, pid,struct pfstat*, pfstat){

	//Get the task_struct of the given process
	struct task_struct* process_task_struct = NULL;
	char* processname;

	if(process_task_struct == NULL){
		return 1;
	}

	if(pfstat == NULL){
		return 2;
	}

	printk("[PFSTAT] Syscall entered!");

	if(process_task_struct->pfstatMode == 0){

		process_task_struct->pfstatMode = 1;

		processname = process_task_struct->comm;

		printk("[PFSTAT] Process %s is now in PFSTAT mode",processname);

		//Set  all fields of the process pfstat structure to 0 to start counting
		memset(&process_task_struct->pfStats,0,sizeof(struct pfstat));
		memset(pfstat,0,sizeof(struct pfstat));

		return 0;

	}else{


		copy_to_user(pfstat,&process_task_struct->pfStats,sizeof(struct pfstat));

/*
		//Get the process pfstat structure after some time
		pfstat->stack_low = process_task_struct->pfStats.stack_low;
		pfstat->transparent_hugepage_fault = process_task_struct->pfStats.transparent_hugepage_fault;
		pfstat->anonymous_fault = process_task_struct->pfStats.anonymous_fault;
		pfstat->file_fault = process_task_struct->pfStats.file_fault;
		pfstat->swapped_back = process_task_struct->pfStats.swapped_back;
		pfstat->copy_on_write = process_task_struct->pfStats.copy_on_write;
		pfstat->fault_alloced_page = process_task_struct->pfStats.fault_alloced_page;
*/
		//Reset  all fields of the process pfstat structure 
		memset(&process_task_struct->pfStats,0,sizeof(struct pfstat));

		return 0;

	}

	return -1;
}



SYSCALL_DEFINE1(lockfs, char*, pathname){

    struct path path;
    struct super_block *sb;
    struct msdos_sb_info *sbi;

    kern_path(pathname, LOOKUP_FOLLOW, &path);

    sb  = path.dentry->d_sb;

	sbi = MSDOS_SB(sb);
	sbi->unlock = 0;

	return 0;
}



SYSCALL_DEFINE2(unlockfs, char*, pathname, char*, pw){

    struct path path;
    struct super_block *sb;
    struct msdos_sb_info *sbi;
	struct buffer_head *bh;
	struct fat_boot_sector *fbs;
	uint8_t *tab_pw;
	int i = 0;
	int returncode = 0

    kern_path(pathname, LOOKUP_FOLLOW, &path);

    sb  = path.dentry->d_sb;
	sbi = MSDOS_SB(sb);

	bh = sb_bread(sb, 0);
	if (bh == NULL) {
		fat_msg(sb, KERN_ERR, "unable to read boot sector "
			"to mark fs as dirty");
		returncode = -1;
	}
	
	fbs = (struct fat_boot_sector *) bh->b_data;

	tab_pw = (uint8_t *) &fbs->hidden;

	//Check the password
	while(tab_pw[i] != '\0'){
		if(tab_pw[i] != pw[i]) {
			returncode = 1; //The password is not valid
		}
		i++;
	}

	//The password is valid, so unlock everything
	sbi->unlock = 1;

	mark_buffer_dirty(bh);
	brelse(bh);

	return returncode;
}



SYSCALL_DEFINE3(changepw, char*, pathname, char*, curr_pw, char*, new_pw){

    struct path path;
    struct super_block *sb;
	struct buffer_head *bh;
	struct fat_boot_sector *fbs;
	uint8_t *tab_pw;
	int i = 0;
	int returncode = 0;

    
    kern_path(pathname, LOOKUP_FOLLOW, &path);

    sb  = path.dentry->d_sb;
	bh = sb_bread(sb, 0);

	if (bh == NULL) {
		fat_msg(sb, KERN_ERR, "unable to read boot sector "
			"to mark fs as dirty");
		returncode = -1;
	}

	fbs = (struct fat_boot_sector *) bh->b_data;

	if(curr_pw == NULL && fbs->hidden != 0){
		returncode = 1;

    }else if(curr_pw != NULL){
    	tab_pw = (uint8_t *) &fbs->hidden;

    	//Check the password
    	while(curr_pw[i] != '\0'){
    		if(tab_pw[i] != curr_pw[i]){
    			returncode = 2; //The passord is not valid
    		}
    		i++;
    	}

    	//The password is valid, so set the new password
    	i=0;
    	while(new_pw[i] != '\0'){
    		tab_pw[i] = new_pw[i];
    		i++;
    	}
		fbs->hidden = *tab_pw;
   }

	mark_buffer_dirty(bh);
	brelse(bh);

    return returncode;
}