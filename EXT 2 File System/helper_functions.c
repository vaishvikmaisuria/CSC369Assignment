#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "ext2.h"
#include "helper_functions.h"

char file_mode(unsigned short i_mode){
	// check the type using bit operation & and given Type field for file mode in .h file.
    char type;
    //check for regular file
    if (i_mode & EXT2_S_IFREG){type = 'f';}
    // check for directory
    else if (i_mode & EXT2_S_IFDIR){type = 'd';}
    //check for symbolic link
    else if (i_mode & EXT2_S_IFLNK){type = 's';}
    // regularly shouldn't reach here ? Error
    else{type = '0';}
	return type;
}

char directory_file_type(struct ext2_dir_entry *entry){
	char type;
	switch(entry->file_type){
		//check for file
		case 1:type = 'f';break;
		// check for directory
		case 2:type = 'd';break;
		//check for symbolic link
		case 7:type = 's';break;
		// regularly shouldn't reach here ? Error
		default:type = '0';break;
	}
	return type;
}


/*Check if the inode is in use using the bitmap 
    returns 1 if the inode is in use. 
*/
int valid_inode(unsigned char block, int index){
    unsigned char mask = (1 << index) & block;
    if (mask) {
        return 1;
    }else{
        return 0;
    }
}

int free_block(unsigned char block, int index){
	unsigned char mask = (1 << index) & ~block;
    if (mask) {
        return 1;
    }else{
        return 0;
    }
}

unsigned char make_inode_valid(unsigned char block, int index){
	//if index is 0 we want to change 7
	//if index is 7 we want to change 0
	return block | (1 << index);
}

unsigned char make_inode_invalid(unsigned char block, int index){
	//if index is 0 we want to change 7
	//if index is 7 we want to change 0
	//int shift = (1 << (7-index));
	return block & ~(1 << index);
}


// Given a File path split the file path  and get the name of the file.
struct LL *file_path_list(char *path){
	LL *directory = malloc(sizeof(struct LL));
	if(strcmp(path,"/")==0){
		strcpy(directory->name,"root");
		directory->next = NULL;
		directory->last = 2;
		return directory;
	}
    char *split_name = strtok(path, "/");
	strcpy(directory->name,split_name);
	directory->last = 0;
	LL *first = directory;
    while (split_name != NULL){
        split_name = strtok (NULL, "/");
		if(split_name == NULL){
			directory->last = 1;
			directory->next = NULL;
			break;
		}
		LL *new_node = malloc(sizeof(struct LL));
		strcpy(new_node->name,split_name);
		
		directory->next = new_node;
		directory->last = 0;
		directory = directory->next;
    }
    return first;
}

// check the length of a name
int check_name_len (char * name){
    int length = strlen(name);
    if (length > EXT2_NAME_LEN){
        return -1;
    }
    return length;

}

 
// pad the reclen to the next muliple of 4
int pad_rec_len(int rec_length){
	int remainder = rec_length % 4;
    if(remainder != 0){
        rec_length += (4 - remainder);
    }
    return rec_length;
}


int find_valid_indode_index(unsigned char *ibm, int first_inode, int inode_count){
	for (int i = first_inode; i < inode_count; i++){
        // c can not point to bits hence we have to shift occurding to bytes(char) (8bits = bytes) - so the block index and offesets
        // single bit we want to find in a block of 8 bits 
        int block_offset = (i - 1) %8;
        // to break the char into blocks of 8
        int block_index = (i - 1) / 8;
        //check if the inode is invalid hence not in use
        if(valid_inode(ibm[block_index], block_offset)){
			return  block_index * 8 + block_offset + 1;
        }
    }
	//nothing is valid
	return -1;
}

int find_invalid_indode_index(unsigned char *ibm, int first_inode, int inode_count){
	for (int i = first_inode; i < inode_count; i++){
        // c can not point to bits hence we have to shift occurding to bytes(char) (8bits = bytes) - so the block index and offesets
        // single bit we want to find in a block of 8 bits 
        int block_offset = (i - 1) %8;
        // to break the char into blocks of 8
        int block_index = (i - 1) / 8;
        //check if the inode is invalid hence not in use
        if(valid_inode(ibm[block_index], block_offset) == 0){
			return  block_index * 8 + block_offset + 1;
        }
    }
	//nothing is invalid
	return -1;
}

int find_free_block_index(unsigned char *bbm, int block_count){
	for (int i = 0; i < block_count; i++){
        // c can not point to bits hence we have to shift occurding to bytes(char) (8bits = bytes) - so the block index and offesets
        // single bit we want to find in a block of 8 bits 
        int block_offset = (i - 1) %8;
        // to break the char into blocks of 8
        int block_index = (i - 1) / 8;
        //check if the inode is invalid hence not in use
        if(free_block(bbm[block_index], block_offset)){
			bbm[block_index] = make_inode_valid(bbm[block_index], block_offset);
			return  block_index * 8 + block_offset + 1;
        }
    }
	//no block is free
	return -1;
}


int path_exists(unsigned char *disk, char *path){
	struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);
	//linked list to store all the elements of a path
	LL *list = file_path_list(path);
	// 1 if path exists 0 if path is invalid
	int result = recursive_path(inode_table, EXT2_ROOT_INO,list,disk);
	free_LL(list);
	return result;
}

void free_LL(LL *list){
	if(list->last == 1){
		free(list);
		return;
	}
	free_LL(list->next);
	free(list);
}

int recursive_path(struct ext2_inode *inode_table, int index, struct LL *list,unsigned char *disk){
	//inode count starts at 1
	int offset = index-1;
    int blocks = inode_table[offset].i_blocks;
	//if we are looking at a directory, as we should be, otherwise something went horribly wrong
	if (inode_table[offset].i_mode & EXT2_S_IFDIR){		
		//go through all the blocks the inode points to
		for (int i = 0; i < blocks-1; i++){
			//make a physical struct of the entry we want to examine
			struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + 1024 * inode_table[offset].i_block[i]);
			int counter = 0;
			//loop through all the entries
			while(counter < 1024){
				char name[dir_entry->name_len];
				strncpy(name,dir_entry->name, dir_entry->name_len);
				//if this file is the last element of the linked list, we validated the path
				if(strncmp(name, list->name, dir_entry->name_len) == 0){
					//if the entry is a file
					if (dir_entry->file_type == 1){
						//if it's not the last element of the linked list, this is a failure
						if(!list->last){
							//this is a file and not the last element
							return 0;
						}
						//this is a file and last element
						return 1;
					}
					//if the entry is a directory
					else if (dir_entry->file_type == 2){
						//if the names of the linked list element and directory match, we might want to go inside it
						if(strcmp(dir_entry->name,list->name) == 0){
							//if this is the last entry in the path, this is not a file, this is a failure
							if(list->last){
								//directory entry and last element
								return 2;
							}
							//recursively follow go into the directory using the next element of the linked list
							list =list->next;
							return recursive_path(inode_table, dir_entry->inode, list,disk);
						}
					}
				}
				//increment counter and move on to the next element
				counter+= dir_entry->rec_len;
				dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry) + dir_entry->rec_len);
			}
		}
	}
	if(list->last){
		//maybe we can use this as a new entry
		return 3;
	}
	//couldn't find a file with this name
	return -1;
}


struct ext2_dir_entry* get_struct(unsigned char *disk, struct ext2_inode *inode_table, int index, LL *list){
	int offset = index -1;
    int blocks = inode_table[offset].i_blocks;
	//if we are looking at a directory, as we should be, otherwise something went horribly wrong
	if (inode_table[offset].i_mode & EXT2_S_IFDIR){
		// This is the blocks which contain the information
		for (int i = 0; i < blocks/2; i++){
			struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * inode_table[offset].i_block[i]);
			int counter = 0;
			//go through all directory entries
			while(counter < EXT2_BLOCK_SIZE){
				char name[dir_entry->name_len];
				strncpy(name,dir_entry->name, dir_entry->name_len);
				if(strncmp(name,list->name, dir_entry->name_len) == 0){
					if(dir_entry->file_type == EXT2_FT_REG_FILE){ // removing a file
						if(list->last){
							return dir_entry;
						}
						else{
							return NULL;
						}
					}
					else if(dir_entry->file_type == EXT2_FT_DIR){
						if(list->last){
							return NULL;
						}
						else{
							list =list->next;
							return get_struct(disk, inode_table, dir_entry->inode, list);
						}
					}
				}
				//if we are at the end of the directory
				if(counter+dir_entry->rec_len >= EXT2_BLOCK_SIZE){
	
					return NULL;
				}
				counter+=dir_entry->rec_len;
				dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry) + dir_entry->rec_len);
			}
		}
	}
	return NULL;
}

void set_default_inode_info(struct ext2_inode *inode_table, int offset){
	inode_table[offset].i_uid = 0;
	
	inode_table[offset].i_gid = 0;
	
	inode_table[offset].osd1 = 0;
	
	inode_table[offset].i_generation = 0;
	inode_table[offset].i_file_acl = 0;
	inode_table[offset].i_dir_acl = 0;
	inode_table[offset].i_faddr = 0;
}

void add_initial_directories(unsigned char *dir_address, int index, int parent){
	//Add the . self inode directory to our new directory
	struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) dir_address;
	dir_entry->inode = index;
	dir_entry->rec_len = 12;
	dir_entry->name_len = 1;
	dir_entry->file_type = EXT2_FT_DIR;
	strncpy(dir_entry->name,".",1);
	dir_address+=12;
	
	//add the .. parent inode directory in our new directory
	dir_entry = (struct ext2_dir_entry *) dir_address;
	dir_entry->inode = parent;
	dir_entry->rec_len = EXT2_BLOCK_SIZE-12;
	dir_entry->name_len = 2;
	dir_entry->file_type = EXT2_FT_DIR;
	strncpy(dir_entry->name,"..",2);
}

int new_directory_fits(struct ext2_dir_entry *last_directory,int counter,char* name){
	//set the size of the last directory to an appropriate multiple of 4
	int lastdirsize = sizeof(struct ext2_dir_entry)+last_directory->name_len;
	lastdirsize = pad_rec_len(lastdirsize);
	
	int curdirsize = sizeof(struct ext2_dir_entry)+check_name_len(name);
	curdirsize = pad_rec_len(curdirsize);
	if(counter+lastdirsize+curdirsize<= EXT2_BLOCK_SIZE){
		return 1;
	}
	return 0;
}

// Gives the name of the target path
char *get_target_file_name (char *target_path){
    
    char *path_name = strtok(target_path, "/");
    char *next_name = path_name;
    
    while(next_name != NULL) {
        path_name = next_name;
        next_name = strtok(NULL, "/");
    }
    return path_name;
}

int pad_file_len(int fsize){
    
    int num = fsize / 1024;
    int remainder1 = fsize % 1024;
    
    if(remainder1 != 0){
        num += 1;
    }
    return num;
}

int check_all_inodes_invalid(struct ext2_inode *inode_table, int offset, unsigned char * disk,unsigned char *bbm, unsigned int *indirect){
	int blocks = inode_table[offset].i_blocks;
	for (int i = 0; i < blocks /2; i++){
        if(i <= 12){
			int block_num = inode_table[offset].i_block[i];
			block_num -= 1;
			if(valid_inode(bbm[block_num/8], block_num%8)){
				return 0;
			}
        }
        else if(i > 12){
			int block_num = indirect[i-13];
			block_num -= 1;
			if(valid_inode(bbm[block_num/8], block_num%8)){
				return 0;
			}
        }
    }
	return 1;
}
