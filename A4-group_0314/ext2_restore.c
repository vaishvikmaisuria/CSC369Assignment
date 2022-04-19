#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include "ext2.h"
#include "helper_functions.h"

unsigned char *disk;
/*
void print_bitmap(unsigned char *bm, int block_num){
        for (int j = 0; j < 8;j++){
            unsigned mask = bm[block_num]  & (1 << j);
            // look at the jth bit 
            if(mask){
                printf("1");
            }else{
                printf("0");
            }
        }
	printf("\n");
}
*/

int restore_file(struct ext2_dir_entry *victim,int last_size){
	//printf("got inside the function\n");
	//printf("last size: %d\n",last_size);
	struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
	struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);
	unsigned char *bbm = (unsigned char *)(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE);
	unsigned char *ibm = (unsigned char *)(disk + gd->bg_inode_bitmap * EXT2_BLOCK_SIZE);
	
	struct ext2_dir_entry* last_entry = (struct ext2_dir_entry*)(((char*)victim) - last_size);
	last_entry->rec_len = last_size;
	
	int index = victim->inode;
	int offset = index - 1;
	int blocks = inode_table[offset].i_blocks;
	unsigned int *indirect = (unsigned int *)(disk + inode_table[offset].i_block[12]*EXT2_BLOCK_SIZE);
	if(!check_all_inodes_invalid(inode_table, offset, disk, bbm, indirect)){
		exit(ENOENT);
	}
	
	if(inode_table[offset].i_mode == EXT2_FT_DIR){
		exit(EISDIR);
	}
	
	if(victim->file_type == EXT2_FT_REG_FILE || victim->file_type == EXT2_FT_SYMLINK){
		if(inode_table[offset].i_links_count >= 1 && victim->file_type != EXT2_FT_SYMLINK){
			inode_table[offset].i_links_count = inode_table[offset].i_links_count+1;
		}
		else{
			for (int i = 0; i < blocks /2; i++){
				if(i <= 12){
					int block_num = inode_table[offset].i_block[i];
					block_num -= 1;
					bbm[block_num/8] = make_inode_valid(bbm[block_num/8], block_num%8);
				}
				else if(i > 12){
					int block_num = indirect[i-13];
					block_num -= 1;
					bbm[block_num/8] = make_inode_valid(bbm[block_num/8], block_num%8);
				}
				sb->s_free_blocks_count -= 1;
				gd->bg_free_blocks_count -= 1;
				
				inode_table[offset].i_dtime = 0;
			}
		}
	}
	
	
	index -= 1;
	ibm[index/8] = make_inode_valid(ibm[index/8], index%8);
	sb->s_free_inodes_count -= 1;
	gd->bg_free_inodes_count -= 1;

	
	inode_table[offset].i_links_count++;

	return victim->rec_len;
}

int search_entry(struct ext2_inode *inode_table, int index, struct LL *list){
	struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
	unsigned char *ibm = (unsigned char *)(disk + gd->bg_inode_bitmap * EXT2_BLOCK_SIZE);
    // the i_node number starts at one whereas the blocks in the disk start at 0
    int offset = index -1;
	//if we are looking at a directory, as we should be, otherwise something went horribly wrong
	if (inode_table[offset].i_mode & EXT2_S_IFDIR){
		// This is the blocks which contain the information
		for (int i = 0; i < inode_table[offset].i_blocks/2; i++){
			struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * inode_table[offset].i_block[i]);
			int counter = 0;
			int last_size = 0;
			//go through all directory entries
			while(counter < EXT2_BLOCK_SIZE){
				//if string is the same as list element, we might want to go into that directory
				
				//printf("found entry %s\n",dir_entry->name);
				int index = dir_entry->inode;
				//printf("inode: %d\n",index);
				index -= 1;
				
				if(dir_entry->file_type == EXT2_FT_DIR){ // restoring a directory 
					if(strcmp(dir_entry->name,list->name) == 0){
						if(list->last){ // last entry
							return 1;
						}
						else{
							list =list->next;
							return search_entry(inode_table, dir_entry->inode, list);
						}
					}
				}
				
				else{ // removing a file
					char name[dir_entry->name_len];
					strncpy(name,dir_entry->name, dir_entry->name_len);
					//printf("COMPARING %s and %s\n",name,list->name );
					if(strncmp(name,list->name, dir_entry->name_len) == 0){
						//printf("Comparison passed\n");
						if(list->last){ // last entry
							//printf("Trying to restore this file\n");
							restore_file(dir_entry,last_size);
							return 0;
						}
						else{
							return 0; //its a file and not the last element?
						}
					}
					//printf("Comparison not passed\n");
				}
				
				
				if(valid_inode(ibm[index/8], index%8)){
					int sizeofdir = sizeof(struct ext2_dir_entry)+dir_entry->name_len;
					sizeofdir = pad_rec_len(sizeofdir);
					counter+=sizeofdir;
					dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry) + sizeofdir);
					last_size = sizeofdir;
				}
				else{
					counter += dir_entry->rec_len;
					last_size += dir_entry->rec_len;
					dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry) + dir_entry->rec_len);
				}
				//printf("counter: %d\n",counter);
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
	
	char * copy_of_destination_path = malloc(strlen(path) + 1);
	strncpy(copy_of_destination_path, path, strlen(path));
	
	int exists = path_exists(disk, copy_of_destination_path);
	
	if(exists == 0 || exists == -1){
		exit(ENOENT);
	}
	
	free(copy_of_destination_path);
	
	return search_entry(inode_table, EXT2_ROOT_INO,list);
	
}
