//
//  ext2_checker.c
//  
//
//  Created by Vaishvik Maisuria on 2018-11-30.
//
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include "helper_functions.h"


unsigned char *disk;


/*
 the superblock and block group counters for free blocks and free inodes must match the number of free inodes and data blocks as indicated
 in the respective bitmaps. If an inconsistency is detected, the checker will trust the bitmaps and update the counters.
 Once such an inconsistency is fixed, your program should output the following message: "Fixed: X's Y counter was off by Z compared to the bitmap",
 where X stands for either "superblock" or "block group", Y is either "free blocks" or "free inodes", and Z is the difference (in absolute value).
 The Z values should be added to the total number of fixes.
 */

int group_counter_inconsis(){
    
    int total_fixs = 0;
    
    
    // check and compare the inode bitmap with the superblock and block group
    
    // block group descriptors counters
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    //starting index for i_node bitmap
    unsigned char *ibm = (unsigned char *)(disk + gd->bg_inode_bitmap * EXT2_BLOCK_SIZE);
    //super block
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    //get the inode count
    int iblock_count = sb->s_inodes_count;
    
    int num_free_inode = 0;
    
    for(int i =0; i < iblock_count;i++){
        for (int j = 0; j < 8;j++){
            unsigned mask = ibm[i]  & (1 << j);
            // look at the jth bit
            if(!mask){
                num_free_inode++;
            }
        }
    }
    
    int diff_inode_sb = ((int)sb->s_free_inodes_count - num_free_inode);
    int diff_inode_gd = ((int)gd->bg_free_inodes_count - num_free_inode);
    
    
    diff_inode_sb = abs(diff_inode_sb);
    diff_inode_gd = abs(diff_inode_gd);


    total_fixs = total_fixs + diff_inode_sb + diff_inode_gd;
    
    if(diff_inode_sb != 0){
        sb->s_free_inodes_count = num_free_inode;
        printf("Fixed: superblock's free inodes counter was off by %d compared to the bitmap", diff_inode_sb);
    }
    
    
    if(diff_inode_gd != 0){
        gd->bg_free_inodes_count = num_free_inode;
        printf("Fixed: block group's free inodes counter was off by %d compared to the bitmap", diff_inode_gd);
    }
    
    // check and compare the block bitmap with the superblock and block group
    
    //starting index for block bitmap
    unsigned char *bbm = (unsigned char *)(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE);
    // get the block count
    int bblock_count = sb->s_blocks_count;
    
    int num_free_blocks = 0;
    
    for(int i =0; i < bblock_count;i++){
        for (int j = 0; j < 8;j++){
            unsigned mask = bbm[i]  & (1 << j);
            // look at the jth bit
            if(!mask){
                num_free_blocks++;
            }
        }
    }
    
    

    int diff_block_sb = ((int)sb->s_free_blocks_count - num_free_blocks);
    int diff_block_gd = ((int)gd->bg_free_blocks_count - num_free_blocks);
    
    diff_block_sb = abs(diff_block_sb);
    diff_block_gd = abs(diff_block_gd);

    total_fixs = total_fixs + diff_block_sb + diff_block_gd;
    
    if(diff_block_sb != 0){
        sb->s_free_blocks_count = num_free_blocks;
        printf("Fixed: superblock's free blocks counter was off by %d compared to the bitmap\n", diff_block_sb);
    }
    if (diff_block_gd != 0){
        gd->bg_free_blocks_count = num_free_blocks;
        printf("Fixed: block group's free blocks counter was off by %d compared to the bitmap\n", diff_block_gd);
    }
    
    return total_fixs;
}

/*
 
 for each file, directory, or symlink, you must check if its inode's i_mode matches the directory entry file_type. If it does not,
 then you shall trust the inode's i_mode and fix the file_type to match. Once such an inconsistency is repaired,
 your program should output the following message: "Fixed: Entry type vs inode mismatch: inode [I]",
 where I is the inode number for the respective file system object. Each inconsistency counts towards to total number of fixes.
 
 */

int fix_file_type(struct ext2_dir_entry *dir_entry){
    
    // block group descriptors counters
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);
    //get the inode
    struct ext2_inode *inode = inode_table + dir_entry->inode -1;
    
    
    int fixed = 0;
 
    
    if((inode->i_mode >> 12) == (EXT2_S_IFREG >> 12)){
        if(dir_entry->file_type !=  EXT2_FT_REG_FILE){
            dir_entry->file_type = EXT2_FT_REG_FILE;
            fixed++;
            printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir_entry->inode);
        }
    }
    else if((inode->i_mode >> 12) == (EXT2_S_IFDIR >> 12)){
        if(dir_entry->file_type != EXT2_FT_DIR){
            dir_entry->file_type = EXT2_FT_DIR;
            fixed++;
            printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir_entry->inode);
        }
    }
    else if((inode->i_mode >> 12) == (EXT2_S_IFLNK>> 12)){
        if(dir_entry->file_type != EXT2_FT_SYMLINK){
            dir_entry->file_type = EXT2_FT_SYMLINK;
            fixed++;
            printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir_entry->inode);
        }
        
    }else{
        if(dir_entry->file_type != EXT2_FT_UNKNOWN){
            dir_entry->file_type = EXT2_FT_UNKNOWN;
            fixed++;
            printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir_entry->inode);
        }
    }
  
    return fixed;
}


/*Check if the inode is in use using the bitmap 
 returns 1 if the inode is in use.
 */
int valid_inodeV2(unsigned char block, int index){
    
    unsigned char mask = (1 << index) & block;

    if (mask) {
        
        return 1;
    }else{
        
        return 0;
    }
}


unsigned char set_in_use(unsigned char block, int index){
    block |= 1 << index;
    // block group descriptors counters
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    gd->bg_free_blocks_count--;
    //super block
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    sb->s_free_blocks_count--;
	return block;
}


/*
 for each file, directory or symlink, you must check that its inode is marked as allocated in the inode bitmap. If it isn't,
 then the inode bitmap must be updated to indicate that the inode is in use. You should also update the corresponding counters in the block group
 and superblock (they should be consistent with the bitmap at this point). Once such an inconsistency is repaired,
 your program should output the following message: "Fixed: inode [I] not marked as in-use", where I is the inode number.
 Each inconsistency counts towards to total number of fixes.
 
 */
int fix_inode_bitmap(struct ext2_dir_entry *dir_entry){
    // block group descriptors counters
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    
    //get the inode
    //struct ext2_inode *inode = inode_table + dir_entry->inode -1;
    //starting index for i_node bitmap
    unsigned char *ibm = (unsigned char *)(disk + gd->bg_inode_bitmap * EXT2_BLOCK_SIZE);
    
    int fixed = 0;
    
    int i_index = dir_entry->inode;
	
	int offset = i_index -1;
    // c can not point to bits hence we have to shift occurding to bytes(char) (8bits = bytes) - so the block index and offesets
    // single bit we want to find in a block of 8 bits
    int block_offset = offset %8;
    // to break the char into blocks of 8
    int block_index = offset / 8;
    
    if(!valid_inodeV2(ibm[block_index], block_offset)){
        ibm[block_index] = make_inode_valid(ibm[block_index], block_offset);
        // block group descriptors counters
        struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
        gd->bg_free_inodes_count--;
        //super block
        struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
        sb->s_free_inodes_count--;
        fixed++;
        printf("Fixed: inode [%d] not marked as in-use\n", dir_entry->inode);
    }
    //printf("The value for fix_inode_bitmap %d\n", fixed);
    return fixed;
}

/*
 for each file, directory, or symlink, you must check that its inode's i_dtime is set to 0. If it isn't, you must reset (to 0),
 to indicate that the file should not be marked for removal. Once such an inconsistency is repaired, your program should output the following message:
 "Fixed: valid inode marked for deletion: [I]", where I is the inode number. Each inconsistency counts towards to total number of fixes.
 
 */
int fix_inode_dtime(struct ext2_dir_entry *dir_entry){
    // block group descriptors counters
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);
    //get the inode
    struct ext2_inode *inode = inode_table + dir_entry->inode -1;
    
    int fixed = 0;
    
    if(inode->i_dtime) {
        inode->i_dtime = 0;
        fixed++;
        printf("Fixed: valid inode marked for deletion: [%d]\n", dir_entry->inode);
    }
    //printf("The value for fix_inode_dtime %d\n", fixed);
    return fixed;
}


/*
 for each file, directory, or symlink, you must check that all its data blocks are allocated in the data bitmap. If any of its blocks is not allocated,
 you must fix this by updating the data bitmap. You should also update the corresponding counters in the block group and superblock,
 (they should be consistent with the bitmap at this point). Once such an inconsistency is fixed, your program should output the following message:
 "Fixed: D in-use data blocks not marked in data bitmap for inode: [I]", where D is the number of data blocks fixed, and I is the inode number. Each inconsistency counts towards to total number of fixes.
 
 */
int fix_data_bitmap(struct ext2_dir_entry *dir_entry){
    // block group descriptors counters
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);
    //starting index for block bitmap
    unsigned char *bbm = (unsigned char *)(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE);
    //printf(" name=%.*s\n", dir_entry->name_len, dir_entry->name);
    //get the inode
    struct ext2_inode *inode = inode_table + dir_entry->inode - 1;
    int total_fixed =0;
    
    int blocks = inode->i_blocks;
    //printf("the blocks total is %d\n", blocks);
    for (int i = 0; i < blocks/2 ; i++){
        if(i<12){
            
            // single bit we want to find in a block of 8 bits
            int block_offset = ((((int)(inode->i_block[i]) -1) % 8 ));
            // to break the char into blocks of 8
            int block_index = (((int)(inode->i_block[i]) -1) / 8);
            //printf("the block index : %d\n", block_index);
            //printf("the block offset : %d\n", block_offset);
            //printf("the block  : %d\n", inode->i_block[i]);

            if (inode->i_block[i] != 0 && !valid_inodeV2(bbm[block_index ], block_offset )){
                bbm[block_index] = set_in_use(bbm[block_index], block_offset );
                total_fixed++;
            }
        }
        //take care of the indirect pointers
        if (i == 12) {
            // the indirect pointer
            unsigned int *indirect_block = (unsigned int *)(disk + (inode->i_block)[12]*EXT2_BLOCK_SIZE);
             
            // single bit we want to find in a block of 8 bits
            int indirblock_offset = indirect_block[12] % 8;
            // to break the char into blocks of 8
            int indirblock_index = indirect_block[12] / 8;

            if ( !(valid_inodeV2(bbm[indirblock_index], indirblock_offset))){
                bbm[indirblock_index] = set_in_use(bbm[indirblock_index], indirblock_offset);
                total_fixed++;
            }

        }

        if ( i > 12 ){
            unsigned int *indirect_block = (unsigned int *)(disk + (inode->i_block)[12]*EXT2_BLOCK_SIZE);
            
            // single bit we want to find in a block of 8 bits
            int indirblock_offset2 = indirect_block[i-13] %8;
            // to break the char into blocks of 8
            int indirblock_index2 = indirect_block[i-13] / 8;

            if (!(valid_inodeV2(bbm[indirblock_index2], indirblock_offset2))){
                bbm[indirblock_index2] = set_in_use(bbm[indirblock_index2], indirblock_offset2);
                total_fixed++;
            }
        }
    }
    
    if(total_fixed){
        printf("Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]\n", total_fixed, dir_entry->inode);
    }
    
    
    return total_fixed;
}

int in_list(LL *list, int inode){
	LL *first = list;
	while(list->next != NULL){
		if(list->last == inode){
			return 1;
		}
		list = list->next;
	}
	if(list->last == inode){
			return 1;
		}
	LL *new_node = malloc(sizeof(struct LL));
	list->next = new_node;
	list = list->next;
	list->last = inode;
	list->next = NULL;
	list = first;
	return 0;
}



int check_block(struct ext2_inode *inode_table, int index, LL *list){
    // the inode number starts at one whereas the blocks in the disk start at 0
    int offset = index -1;
    int fixed=0;
    int blocks = inode_table[offset].i_blocks;
	
	if (inode_table[offset].i_mode & EXT2_S_IFDIR){
		// This is the blocks which contain the information
		for (int i = 0; i < blocks/2; i++){
			struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + 1024 * inode_table[offset].i_block[i]);
			int counter = 0;
            while(counter < 1024){
				//do the calculations
                // return for the . and ..
               
			    if(!in_list(list,dir_entry->inode)){
					fixed += fix_file_type(dir_entry) + 
							 fix_inode_bitmap(dir_entry) +  
							 fix_inode_dtime(dir_entry) + 
							 fix_data_bitmap(dir_entry);
				}
				counter += dir_entry->rec_len;
				dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry) + dir_entry->rec_len);
			}
		}
        //printf("THE CALCULATION: %d\n", fixed);
	}
    return fixed;
}


int main(int argc, char *argv[]){
    
    // check the args for cp 1: ./ext2_ln 2: disk image
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name> >\n", argv[0]);
        exit(1);
    }
    
    // Get the disk
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    
    int total_fixed_num = 0;
    
    total_fixed_num += group_counter_inconsis();
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    // block group descriptors counters
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);

    // we got to jump to the next valid inode IN USE -  First non-reserved inode 
    int first_inode = sb->s_first_ino + 1;
    // number of inode which are present that can be used to how a block;
    int inode_count = sb->s_inodes_count;

	
	LL *list = malloc(sizeof(struct LL));
	list->next = NULL;
	
	
    total_fixed_num += check_block(inode_table, EXT2_ROOT_INO, list);
	
    
	for (int i = first_inode; i < inode_count; i++){
        // c can not point to bits hence we have to shift occurding to bytes(char) (8bits = bytes) - so the block index and offesets
        // single bit we want to find in a block of 8 bits 
        int block_offset = (i - 1) %8;
        // to break the char into blocks of 8
        int block_index = (i - 1) / 8;
        //check if the inode is valid hence in use
       
	   
	   
        total_fixed_num += check_block(inode_table, block_index * 8 + block_offset + 1, list);
        
    }

    if(total_fixed_num){
        printf("%d file system inconsistencies repaired!\n", total_fixed_num);
    }else{
        printf("No file system inconsistencies detected!\n");
    }
    
    
    return 0;
}
