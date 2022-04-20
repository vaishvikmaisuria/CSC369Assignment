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


int remove_file(struct ext2_dir_entry *victim,int last_size){
	//printf("got inside the function\n");
	struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
	struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);
	unsigned char *bbm = (unsigned char *)(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE);
	unsigned char *ibm = (unsigned char *)(disk + gd->bg_inode_bitmap * EXT2_BLOCK_SIZE);
	
	struct ext2_dir_entry* last_entry = (struct ext2_dir_entry*)(((char*)victim) - last_size);
	last_entry->rec_len = last_size + victim->rec_len;

	int index = victim->inode;
	int offset = index - 1;
	int blocks = inode_table[offset].i_blocks;
	
	if(victim->file_type == EXT2_FT_REG_FILE || victim->file_type == EXT2_FT_SYMLINK){
		if(inode_table[offset].i_links_count > 1 && victim->file_type != EXT2_FT_SYMLINK){
			inode_table[offset].i_links_count = inode_table[offset].i_links_count-1;
		}
		else{
			for (int i = 0; i < blocks /2; i++){
				if(i <= 12){
					int block_num = inode_table[offset].i_block[i];
					block_num -= 1;
					bbm[block_num/8] = make_inode_invalid(bbm[block_num/8], block_num%8);
				}
				else if(i > 12){
					unsigned int *indirect = (unsigned int *)(disk + inode_table[offset].i_block[12]*EXT2_BLOCK_SIZE);
					int block_num = indirect[i-13];
					block_num -= 1;
					bbm[block_num/8] = make_inode_invalid(bbm[block_num/8], block_num%8);
				}
				sb->s_free_blocks_count += 1;
				gd->bg_free_blocks_count += 1;
			}
			
			index -= 1;
			ibm[index/8] = make_inode_invalid(ibm[index/8], index%8);
			sb->s_free_inodes_count += 1;
			gd->bg_free_inodes_count += 1;
			

			//update inode information
			inode_table[offset].i_dtime = (unsigned)time(NULL);
			inode_table[offset].i_links_count -= 1;
		}
	}
	return victim->rec_len;
}


int search_entry(struct ext2_inode *inode_table, int index, struct LL *list){
    // the i_node number starts at one whereas the blocks in the disk start at 0
    int offset = index -1;
    int blocks = inode_table[offset].i_blocks;
	//if we are looking at a directory, as we should be, otherwise something went horribly wrong
	if (inode_table[offset].i_mode & EXT2_S_IFDIR){
		// This is the blocks which contain the information
		for (int i = 0; i < blocks-1; i++){
			struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * inode_table[offset].i_block[i]);
			int counter = 0;
			int last_size = 0;
			//go through all directory entries
			while(counter < EXT2_BLOCK_SIZE){
				//if string is the same as list element, we might want to go into that directory
				//printf("looking at entry %s\n", dir_entry->name);
				if(dir_entry->file_type == EXT2_FT_DIR){ // removing a directory 
					if(strcmp(dir_entry->name,list->name) == 0){
						if(list->last){ // last entry
							//printf("We would have deleted this directory\n");
							exit(EISDIR);
						}
						else{
							list =list->next;
							//printf("going deeper into directory %s\n",dir_entry->name);
							return search_entry(inode_table, dir_entry->inode, list);
						}
					}
				}
				else{ // removing a file
					//printf("realized this is a file\n");
					char name[dir_entry->name_len];
					strncpy(name,dir_entry->name, dir_entry->name_len);
					if(strcmp(name,list->name) == 0){
						if(list->last){ // last entry
							remove_file(dir_entry,last_size);
							return 1;
						}
						else{
							return 0; //its a file and not the last element?
						}
					}
				}
				last_size = dir_entry->rec_len;
				counter+=dir_entry->rec_len;
				//struct ext2_dir_entry *last_entry = dir_entry;
				dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry) + dir_entry->rec_len);
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
	
	char * copy_of_destination_path = malloc(strlen(path) + 1);
	strncpy(copy_of_destination_path, path, strlen(path));
	
	int exists = path_exists(disk, copy_of_destination_path);
	
	if(exists == 0 || exists == -1){
		exit(ENOENT);
	}
	
	if(exists == 2){
		exit(EISDIR);
	}
	
	free(copy_of_destination_path);
	
	LL *list = file_path_list(path);
	
	search_entry(inode_table, EXT2_ROOT_INO,list);
	
	free_LL(list);
}
