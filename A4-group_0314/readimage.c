#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;

//char pointer is important it will break if you use a int pointer 
void print_bitmap(unsigned char *bm, int block_count){
    for(int i = 0; i < block_count /8; i++){
        for (int j = 0; j < 8;j++){
            unsigned mask = bm[i]  & (1 << j);
            // look at the jth bit 
            if(mask){
                printf("1");
            }else{
                printf("0");
            }
        }
        printf(" ");
    }
	printf("\n");
}




//print the details of the inode and index is the inode number 

void print_inode(struct ext2_inode *inode_table, int index){
    // the inode number starts at one whereas the blocks in the disk start at 0
    int offset = index -1;
	
    // check the type using bit operation & and given Type field for file mode in .h file.
    char type;
    //check for regular file
    if (inode_table[offset].i_mode & EXT2_S_IFREG){
        type = 'f';
    }
    // check for directory
    else if (inode_table[offset].i_mode & EXT2_S_IFDIR){
         type = 'd';
    }
    //check for symbolic link
    else if (inode_table[offset].i_mode & EXT2_S_IFLNK){
        type = 's';
    }
    // regularly shouldn't reach here ? Error
    else{
        type = '0';
    }

    // the size of the inode table 
    int size = inode_table[offset].i_size;
    // the number of links for a inode in the inode table
    int links_count = inode_table[offset].i_links_count;
    // the number of blocks the inode points to 
    int blocks = inode_table[offset].i_blocks;

    //print according to the requirements
    printf("[%i] type: %c size: %i links: %i blocks: %i\n", index, type, size, links_count, blocks);
    printf("[%i] Blocks: ", index);

    // This is the blocks which contain the information
    for (int i = 0; i < blocks /2; i++){
        
        if(i <= 12){
            printf(" %i", inode_table[offset].i_block[i]);
        }

        else if(i > 12){
            unsigned int *indirect =  (unsigned int *)(disk + inode_table[offset].i_block[12]*EXT2_BLOCK_SIZE);
            printf(" %i", indirect[i-13]);
        }

    }
    printf("\n");
}

void print_block(struct ext2_inode *inode_table, int index){
    // the inode number starts at one whereas the blocks in the disk start at 0
    int offset = index -1;

    int blocks = inode_table[offset].i_blocks;
	
	if (inode_table[offset].i_mode & EXT2_S_IFDIR){
		// This is the blocks which contain the information
		for (int i = 0; i < blocks/2; i++){
			printf("   DIR BLOCK NUM:");
			printf(" %i ", inode_table[offset].i_block[i]);
			printf("(for inode %d)\n", index);
			
			struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + 1024 * inode_table[offset].i_block[i]);
			int counter = 0;

            
			
			while(counter < 1024){
				//check for regular file
				char type;
				if (dir_entry->file_type == 1){
					type = 'f';
				}
				// check for directory
				else if (dir_entry->file_type == 2){
					 type = 'd';
				}
				//check for symbolic link
				else if (dir_entry->file_type == 7){
					type = 's';
				}
				// regularly shouldn't reach here ? Error
				else{
					type = '0';
				}
				printf("Inode: %d rec_len: %d name_len: %d type= %c",dir_entry->inode,dir_entry->rec_len,dir_entry->name_len,type);
				printf(" name=%.*s\n", dir_entry->name_len, dir_entry->name);
				counter += dir_entry->rec_len;
				dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry) + dir_entry->rec_len);
			}
			
		}
	}
}

/*Check if the inode is in use using the bitmap 
    returns 1 if the inode is in use. 
*/
int valid_inodeV1(unsigned char *block, int index){
    for(int i = 0; i < index /8; i++){
        for (int j = 0; j < 8;j++){  
            unsigned mask = block[i]  & (1 << j);
            // look at the jth bit 
            if(mask){
                return 1;
            }else{
                return 0;
            }
        }
    }
	return 0;
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

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }


    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);
    
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    printf("Block group:\n");
    printf("    block bitmap: %d\n", gd->bg_block_bitmap);
    printf("    inode bitmap: %d\n", gd->bg_inode_bitmap);
    printf("    inode table: %d\n", gd->bg_inode_table);
    printf("    free blocks: %d\n", gd->bg_free_blocks_count);
    printf("    free inodes: %d\n", gd->bg_free_inodes_count);
    printf("    used_dirs: %d\n", gd->bg_used_dirs_count);
    
    //starting index for block bitmap
    unsigned char *bbm = (unsigned char *)(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE);
    //starting index for i_node bitmap
    unsigned char *ibm = (unsigned char *)(disk + gd->bg_inode_bitmap * EXT2_BLOCK_SIZE);

    printf("Block bitmap: ");
	//loop through bitmap and print their content 
    print_bitmap(bbm, sb->s_blocks_count);
    printf("Inode bitmap: ");
	//loop through inode and print their content 
    print_bitmap(ibm, sb->s_inodes_count);
    
    printf("\n");
    printf("Inodes:\n");
    
    // The whole disk is divided into fixed-sized blocks and we want to access the block that conatins the inode table
    struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);
    
    // print the root inode
    print_inode(inode_table, EXT2_ROOT_INO);

    // we got to jump to the next valid inode IN USE -  First non-reserved inode 
    int first_inode = sb->s_first_ino + 1;
    // number of inode which are present that can be used to how a block;
    int inode_count = sb->s_inodes_count;

    for (int i = first_inode; i < inode_count; i++){
        // c can not point to bits hence we have to shift occurding to bytes(char) (8bits = bytes) - so the block index and offesets
        // single bit we want to find in a block of 8 bits 
        int block_offset = (i - 1) %8;
        // to break the char into blocks of 8
        int block_index = (i - 1) / 8;
        //check if the inode is valid hence in use
        if(valid_inodeV2(ibm[block_index], block_offset)){
            print_inode(inode_table, block_index * 8 + block_offset + 1);
        }
    }
    printf("\n");
    printf("Directory Blocks:\n");
    print_block(inode_table, EXT2_ROOT_INO);
    for (int i = first_inode; i < inode_count; i++){
        // c can not point to bits hence we have to shift occurding to bytes(char) (8bits = bytes) - so the block index and offesets
        // single bit we want to find in a block of 8 bits 
        int block_offset = (i - 1) %8;
        // to break the char into blocks of 8
        int block_index = (i - 1) / 8;
        //check if the inode is valid hence in use
        if(valid_inodeV2(ibm[block_index], block_offset)){
			//printf("PRINTING BLOCK:\n");
            print_block(inode_table, block_index * 8 + block_offset + 1);
        }
    }
	printf("\n");
    return 0;
}
