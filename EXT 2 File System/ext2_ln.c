#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include "ext2.h"
#include "helper_functions.h"


unsigned char *disk;


struct ext2_dir_entry *mk_dir(struct ext2_inode *inode_table, int index, struct LL *list){
    
    // the i_node number starts at one whereas the blocks in the disk start at 0
    int offset = index -1;
    int blocks = inode_table[offset].i_blocks;
    int new_block = 0; //not a new block
    struct ext2_dir_entry *dir_entry =  (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * inode_table[offset].i_block[0]);
    //if we are looking at a directory, as we should be, otherwise something went horribly wrong
  
        // This is the blocks which contain the information
        for (int i = 0; i < blocks-1; i++){
            struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * inode_table[offset].i_block[i]);
            int counter = 0;
            //go through all directory entries
            while(counter < EXT2_BLOCK_SIZE){
                if(new_block){
                    dir_entry->rec_len = 0;
                    new_block = 0;
                    return dir_entry;
                   
                }
               
                //if string is the same as list element, we might want to go into that directory
                if(strcmp(dir_entry->name,list->name) == 0){
                    //if the entry is the last element of the list, this is not a good path
                    if(list->last){
                        break;
                    }
                    // we don't want to recurse into something that is not a directory
                    else if(dir_entry->file_type != EXT2_FT_DIR){
                        break;
                    }
                    
                    //recurse into the new directory
                    list =list->next;
                    return mk_dir(inode_table, dir_entry->inode, list);
                }
                
                counter+=dir_entry->rec_len;
                //struct ext2_dir_entry *last_entry = dir_entry;
                dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry) + dir_entry->rec_len);
            }
            return dir_entry;
        }
    
    return dir_entry;
}
struct ext2_dir_entry *add_hardlink(struct ext2_dir_entry *last_directory, struct ext2_dir_entry *target_dir_entry, char* name,int counter, int flag){
    
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);
    
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
    //create the entry for our EXT2_FT_REG_FILE for hardlink
    strcpy(new_entry->name, name);
    new_entry->name_len= check_name_len(name);


    if(flag){
        
        //Take an unused inode index to get the inode to point at this new directory
        int first_inode = sb->s_first_ino + 1;
        int inode_count = sb->s_inodes_count;
        int index = find_invalid_indode_index(ibm, first_inode, inode_count);
		if(index == -1){
			exit(ENOSPC);
		}
        sb->s_free_inodes_count -= 1;
        gd->bg_free_inodes_count -= 1;
        int offset = index - 1;
        int block_offset = (index - 1) %8;
        int block_index = (index - 1) / 8;
        
        //create the entry 
        
        strcpy(new_entry->name, name);
        new_entry->name_len= check_name_len(name);
        new_entry->file_type= EXT2_FT_SYMLINK;
        new_entry->inode = index;
        //make the length of the directory fill up the block
        
        //make the unused inode we are using, valid
        ibm[block_index] = make_inode_valid(ibm[block_index], block_offset);
        inode_table[offset].i_blocks = 2;
	    inode_table[offset].i_dtime =0;
        inode_table[offset].i_mode = EXT2_S_IFLNK;
        inode_table[offset].i_links_count++;
        new_entry->file_type= EXT2_FT_SYMLINK;
    }
    else{
        new_entry->file_type= EXT2_FT_REG_FILE;
        new_entry->inode = target_dir_entry->inode;
        //get the inode table and incremeant the link count
    (inode_table + target_dir_entry->inode - 1) -> i_links_count++;
    }

    //make the length of the directory fill up the block
    new_entry->rec_len = EXT2_BLOCK_SIZE -counter -last_directory->rec_len;
    
    
    return new_entry;
}


struct ext2_dir_entry *mk_dir2(struct ext2_inode *inode_table, struct ext2_dir_entry *target_dir_entry, int index, struct LL *list, int flag){
    
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    unsigned char *bbm = (unsigned char *)(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE);
    // the i_node number starts at one whereas the blocks in the disk start at 0
    int offset = index -1;
    int blocks = inode_table[offset].i_blocks;
    int new_block = 0; //not a new block
    struct ext2_dir_entry *dir_entry =  (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * inode_table[offset].i_block[0]);
    //if we are looking at a directory, as we should be, otherwise something went horribly wrong
    if (inode_table[offset].i_mode & EXT2_S_IFDIR){
        // This is the blocks which contain the information
        for (int i = 0; i < blocks-1; i++){
            struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * inode_table[offset].i_block[i]);
            int counter = 0;
            //go through all directory entries
            while(counter < EXT2_BLOCK_SIZE){
                if(new_block){
                    dir_entry->rec_len = 0;
                    new_block = 0;
                    return add_hardlink(dir_entry,target_dir_entry, list->name, counter, flag);
                   
                }
                //if string is the same as list element, we might want to go into that directory
                if(strcmp(dir_entry->name,list->name) == 0){
                    //if the entry is the last element of the list, this is not a good path
                    if(list->last){
                        break;
                    }
                    // we don't want to recurse into something that is not a directory
                    else if(dir_entry->file_type != EXT2_FT_DIR){
                        break;
                    }
                    //recurse into the new directory
                    list =list->next;
                    return mk_dir2(inode_table, target_dir_entry, dir_entry->inode, list,flag);
                }
                //if we are at the end of the directory
                if(counter+dir_entry->rec_len >= EXT2_BLOCK_SIZE){
                    //if we are not at the last element of the list, the path is wrong
                    if(!list->last){
                        break;
                    }
                    //if the directory is expected to fit in this block put it in
                    if(new_directory_fits(dir_entry,counter,list->name)){
                        return add_hardlink(dir_entry, target_dir_entry, list->name, counter, flag);
                    }
                    else{
                        if(i+1==blocks-1){
                            //put it in another block
                            int free_block = find_free_block_index(bbm, sb->s_blocks_count);
                            inode_table[offset].i_block[i+2] = inode_table[offset].i_block[i+1];
                            inode_table[offset].i_block[i+1] = free_block;
                            inode_table[offset].i_blocks++;
                            new_block = 1;
                            blocks++;
                        }
                        break;
                    }
                    
                }
                counter+=dir_entry->rec_len;
                
                dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry) + dir_entry->rec_len);
            }
        }
    }
    return add_hardlink(dir_entry, target_dir_entry, list->name, 0, flag);
}

//char pointer is important it will break if you use a int pointer
int print_bitmap(unsigned char *bm, int block_count){
    for(int i = 0; i < block_count /8; i++){
        for (int j = 0; j < 8;j++){
            unsigned mask = bm[i]  & (1 << j);
            // look at the jth bit
            if(!mask){
                
                bm[i] |= 1 << j;
                return (i * 8 + j + 1);
            }
        }
    }
    return 0;
}


int main(int argc, char *argv[]){
    
    // check the args for cp 1: ./ext2_ln 2: disk image 5: optional (s flag) 3: source path 4: destination path
    if(argc < 4 || argc > 5) {
        fprintf(stderr, "Usage: %s <image file name> [-s] <target_file_path> <link_file_path>\n", argv[0]);
        exit(1);
    }

    // Get the disk
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    
    char *target_path;
    char *saved_target_path;
    char *dest_path;

    // two cases: Hard link - 4 argc & Soft Link - 5 argc
    
    //check for hard link
    if(argc == 4){
        // the path that contains the file we want to link
        target_path = argv[2];
        // the path that contains the new locations where the link will exist 
        dest_path = argv[3];
    } 

    //check if the flag for the symbolic link is present 
    if (argc == 5){
        
        char *flag = argv[2];
        char cmp[] = "-s";

        if (strncmp(flag, cmp, 2)){
            printf("fucked\n");
            exit(-1);
        }
      
        // the path that contains the file we want to link
        target_path = argv[3];
        saved_target_path = malloc(strlen(target_path) + 1);
        strncpy(saved_target_path, target_path, strlen(target_path));
        // the path that contains the new locations where the link will exist 
        dest_path = argv[4];
    }
    

    // create a linked list of the target file path
    LL *tlist = file_path_list(target_path);

    // create a linked list of the destination path 
    LL *dlist = file_path_list(dest_path);

    // get the group descriptors
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);

    //The whole disk is divided into fixed-sized blocks and we want to access the block that contains the i_node table
    struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);

    // both cases we have to find the file or directory path dir_entry 
    struct ext2_dir_entry *target_dir_entry = get_struct(disk, inode_table, EXT2_ROOT_INO, tlist);
   
    //checks not done if path exists 

    // destination of the link
     struct ext2_dir_entry *dest_entry;

    //flag for determining if we are doing symbolic link or not
    int flag = 0;
    
    // Hard link
    if(argc == 4){
        // directories can have a hard link 
        if(target_dir_entry->file_type == EXT2_FT_DIR){
            printf("Not Possible to have hard link on directory\n");
        }
        // set the hard link
        dest_entry = mk_dir2(inode_table, target_dir_entry, EXT2_ROOT_INO, dlist, flag);
        
    }

    // symbolic link 
    if(argc == 5){
        flag = 1;
        // create the entry in the direntry for the destination similar to hard link
        dest_entry = mk_dir2(inode_table, target_dir_entry, EXT2_ROOT_INO, dlist, flag);

        struct ext2_inode *dest_inode = (inode_table + dest_entry->inode -1);
        
       
        
        struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
        struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
        //starting index for block bitmap
        unsigned char *bbm = (unsigned char *)(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE);
            
        //allocate resources
        int block_num = print_bitmap(bbm, sb->s_blocks_count);
            
        //update the gd
        gd->bg_free_blocks_count--;
        sb->s_free_blocks_count --;
            
            
        // get the length of the path
        int target_len = strlen(saved_target_path);
            
            
        //zero out the new block ?? 
            
        dest_inode->i_block[0] = block_num;
        dest_inode->i_size = target_len;
            
        unsigned char *blocks = disk + (EXT2_BLOCK_SIZE * block_num);
            
        memcpy(blocks, saved_target_path, target_len);
        free(saved_target_path);
        
    }

    
    return 0;

    
}
