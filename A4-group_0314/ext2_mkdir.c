#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>
#include "ext2.h"
#include "helper_functions.h"

unsigned char *disk;

int add_directory(struct ext2_dir_entry *last_directory,char* name,int counter,int parent){
	
	struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
	struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);
	unsigned char *bbm = (unsigned char *)(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE);
	unsigned char *ibm = (unsigned char *)(disk + gd->bg_inode_bitmap * EXT2_BLOCK_SIZE);
	
	struct ext2_dir_entry* new_entry = last_directory;
	
	//if not a new block
	if(last_directory->rec_len > 0){
		//set the size of the last directory to an appropriate multiple of 4
		int sizeofdir = sizeof(struct ext2_dir_entry)+last_directory->name_len;
		sizeofdir = pad_rec_len(sizeofdir);
		last_directory->rec_len = sizeofdir;
		new_entry = (struct ext2_dir_entry*)(((char*)last_directory) + last_directory->rec_len);
	}
	 
	//Take an unused inode index to get the inode to point at this new directory
    int first_inode = sb->s_first_ino + 1;
    int inode_count = sb->s_inodes_count;
	int index = find_invalid_indode_index(ibm, first_inode, inode_count);
	if(index == -1){
		exit(ENOSPC);
	}
	sb->s_free_inodes_count -= 1;
	gd->bg_free_inodes_count -= 1;
	gd->bg_used_dirs_count += 1;
	int offset = index - 1;
	int block_offset = (index - 1) %8;
	int block_index = (index - 1) / 8;
	
	//create the entry for our new directory
	
	strcpy(new_entry->name, name);
	new_entry->name_len= check_name_len(name);
	new_entry->file_type= EXT2_FT_DIR;
	new_entry->inode = index;
	//make the length of the directory fill up the block
	new_entry->rec_len = EXT2_BLOCK_SIZE -counter -last_directory->rec_len;
	
	//make the unused inode we are using, valid
	ibm[block_index] = make_inode_valid(ibm[block_index], block_offset);
			
	//initialize newly valid inode's values
	inode_table[offset].i_mode = EXT2_S_IFDIR;
	inode_table[offset].i_size = EXT2_BLOCK_SIZE;
	inode_table[offset].i_ctime = (unsigned)time(NULL);
	inode_table[offset].i_links_count = 2;
	inode_table[offset].i_blocks = 2;
	inode_table[offset].i_dtime =0;
	int free_block = find_free_block_index(bbm, sb->s_blocks_count);
	if(free_block == -1){
		exit(ENOSPC);
	}
	sb->s_free_blocks_count -= 1;
	gd->bg_free_blocks_count -= 1;
	inode_table[offset].i_block[0] = free_block;
	inode_table[parent-1].i_links_count += 1;
	
	//create the two initial directories . and ..
	unsigned char *dir_address = disk + free_block * EXT2_BLOCK_SIZE;
	add_initial_directories(dir_address, index, parent);
	set_default_inode_info(inode_table, offset);
	
	
	return new_entry->rec_len;
}

int mk_dir(struct ext2_inode *inode_table, int index, struct LL *list){
	struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
	struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
	unsigned char *bbm = (unsigned char *)(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE);
    // the i_node number starts at one whereas the blocks in the disk start at 0
    int offset = index -1;
	//if we are looking at a directory, as we should be, otherwise something went horribly wrong
	if (inode_table[offset].i_mode & EXT2_S_IFDIR){
		// This is the blocks which contain the information
		for (int i = 0; i < inode_table[offset].i_blocks/2; i++){
			struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * inode_table[offset].i_block[i]);
			int counter = 0;
			//go through all directory entries
			while(counter < EXT2_BLOCK_SIZE){
				//if string is the same as list element, we might want to go into that directory
				if(strcmp(dir_entry->name,list->name) == 0){
					// we don't want to recurse into something that is not a directory
					if(dir_entry->file_type != EXT2_FT_DIR){return 0;}
					//if the entry is the last element of the list, this is not a good path
					if(list->last){exit(EEXIST);}
					//recurse into the new directory
					list =list->next;
					return mk_dir(inode_table, dir_entry->inode, list);
				}
				//if we are at the end of the directory
				if(counter+dir_entry->rec_len >= EXT2_BLOCK_SIZE){
					//if we are not at the last element of the list, the path is wrong
					if(!list->last){return 0;}
					//if the directory is expected to fit in this block put it in
					if(new_directory_fits(dir_entry,counter,list->name)){
						return add_directory(dir_entry,list->name, counter,index);
					}
					if(i+1==inode_table[offset].i_blocks/2){
						//put it in another block
						int free_block = find_free_block_index(bbm, sb->s_blocks_count);
						inode_table[offset].i_block[i+1] = free_block;
						if(free_block == -1){
							exit(ENOSPC);
						}
						inode_table[offset].i_blocks+=2;
						struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * inode_table[offset].i_block[i+1]);
						counter = 0;
						dir_entry->rec_len = 0;
						return add_directory(dir_entry,list->name, counter,index);
					}
					return 0;
				}
				counter+=dir_entry->rec_len;
				dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry) + dir_entry->rec_len);
			}
		}
	}
	return 0;
}

int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <file_path>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
	struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
	// The whole disk is divided into fixed-sized blocks and we want to access the block that contains the i_node table
    struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);
	
	char *path = argv[2];
	LL *list = file_path_list(path);
	
	int exists = path_exists(disk, path);
	
	if(exists != 2 && exists != 3){
		exit(ENOENT);
	}
    return mk_dir(inode_table, EXT2_ROOT_INO,list);
	
	free_LL(list);
}
