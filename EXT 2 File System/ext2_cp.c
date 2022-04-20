//
//  ext2_cp.c
//  
//
//  Created by Vaishvik Maisuria on 2018-11-24.
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
#include <errno.h>


unsigned char *disk;


struct ext2_dir_entry *add_file_loc(struct ext2_dir_entry *last_directory,char* name,int counter,int parent){
    
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
    
    //Take an unused inode index to get the inode to point at this new directory
    int first_inode = sb->s_first_ino + 1;
    int inode_count = sb->s_inodes_count;
    int index = find_invalid_indode_index(ibm, first_inode, inode_count);
    int offset = index - 1;
    int block_offset = (index - 1) %8;
    int block_index = (index - 1) / 8;
    
    //create the entry for our new directory or EXT2_FT_REG_FILE
    
    strcpy(new_entry->name, name);
    new_entry->name_len= check_name_len(name);
    new_entry->file_type= EXT2_FT_REG_FILE;
    new_entry->inode = index;
    //make the length of the directory fill up the block
    new_entry->rec_len = EXT2_BLOCK_SIZE -counter -last_directory->rec_len;
    
    //make the unused inode we are using, valid
    ibm[block_index] = make_inode_valid(ibm[block_index], block_offset);
    
    //initialize newly valid inode's values
    inode_table[offset].i_mode = EXT2_S_IFREG;
    inode_table[offset].i_size = EXT2_BLOCK_SIZE;
    inode_table[offset].i_links_count = 0;
    inode_table[offset].i_blocks = 0;
	inode_table[offset].i_dtime =0;
    //int free_block = find_free_block_index(bbm, sb->s_blocks_count);
    //inode_table[offset].i_block[0] = free_block;
    
    return new_entry;
}


struct ext2_dir_entry *get_file_entry(struct ext2_inode *inode_table, int index, struct LL *list){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    unsigned char *bbm = (unsigned char *)(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE);
    // the i_node number starts at one whereas the blocks in the disk start at 0
    int offset = index -1;
    struct ext2_dir_entry *dir_entry =  (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * inode_table[offset].i_block[0]);
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
                    //if the entry is the last element of the list, this is not a good path
                    if(list->last){break;}
                    // we don't want to recurse into something that is not a directory
                    else if(dir_entry->file_type != EXT2_FT_DIR){break;}
                    //recurse into the new directory
                    list =list->next;
                    return get_file_entry(inode_table, dir_entry->inode, list);
                }
                //if we are at the end of the directory
                if(counter+dir_entry->rec_len >= EXT2_BLOCK_SIZE){
                    //if we are not at the last element of the list, the path is wrong
                    if(!list->last){
                        break;
                    }
                    //if the directory is expected to fit in this block put it in
                    if(new_directory_fits(dir_entry,counter,list->name)){
                        return add_file_loc(dir_entry,list->name, counter,index);
                    }
                    if(i+1==inode_table[offset].i_blocks/2){
                        //put it in another block
                        inode_table[offset].i_block[i+1] = find_free_block_index(bbm, sb->s_blocks_count);
                        inode_table[offset].i_blocks+=2;
                        struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * inode_table[offset].i_block[i+1]);
                        counter = 0;
                        dir_entry->rec_len = 0;
                        return add_file_loc(dir_entry,list->name, counter,index);
                    }
                    return 0;
                }
                counter+=dir_entry->rec_len;
                dir_entry = (struct ext2_dir_entry*)(((char*)dir_entry) + dir_entry->rec_len);
            }
        }
    }
    return add_file_loc(dir_entry,list->name, 0,index);
}

int main(int argc, char *argv[]){
    
    // check the args for cp 1: ./ext2_cp 2: disk image 3: source path 4: destination path
    if(argc != 4) {
        fprintf(stderr, "Usage: %s <image file name> <file_path>\n", argv[0]);
        exit(1);
    }
    
    int fd = open(argv[1], O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    // path that contains the file we want to copy
    char *source_path = argv[2];
    // path to destination where we want to copy the file
    char *destination_path = argv[3];
    

    //copy the shit 
    char proxydpath[1024];
    char proxyspath[1024];
    
    strcpy(proxydpath, destination_path);
    strcpy(proxyspath, source_path);

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    struct ext2_group_desc *gd = (struct ext2_group_desc *) (disk +2 * EXT2_BLOCK_SIZE);
    // The whole disk is divided into fixed-sized blocks and we want to access the block that contains the i_node table
    struct ext2_inode *inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table* EXT2_BLOCK_SIZE);
    //starting index for block bitmap
    unsigned char *bbm = (unsigned char *)(disk + gd->bg_block_bitmap * EXT2_BLOCK_SIZE);

    //open the file that we want to copy
    FILE *source_file = fopen(source_path, "rb");
    if (source_file == NULL) {
        printf("Error: None Existent path");
        exit(ENOENT);
    }
    struct stat buf1;

    unsigned int size_of_file; 
    stat(source_path, &buf1);
    size_of_file = buf1.st_size;
    
    int blocks_used = pad_file_len(size_of_file) + 1;
    
    if(blocks_used > sb->s_free_blocks_count){
        printf("Error: None space for file");
        exit(ENOSPC);
    }
    
    LL *list;
    
    
    //get the file name we should
    char *target_name;
    
    if(strncmp(&proxydpath[strlen(proxydpath)-1], "/", 1)){
        target_name = get_target_file_name(proxydpath);
        list = file_path_list(destination_path);
    }else{
        target_name = get_target_file_name(proxyspath);
        strcat(proxydpath,target_name);
        list = file_path_list(proxydpath);
    }
    
    
    
    // create a file on the disk for the file
    struct ext2_dir_entry *target_file = get_file_entry(inode_table, EXT2_ROOT_INO,list);
    
    //get position of new file inside the inode table using the dir_entry
    struct ext2_inode *new_file = inode_table + (target_file->inode - 1);
    
    gd->bg_free_inodes_count--;
    sb->s_free_inodes_count --;
   
    
    // Finally start copying data from file to inode
    // the blocks in the inode counter
    // reading checker
    int read_chk = 0;
    
    char buf[EXT2_BLOCK_SIZE];
    
    // Write data to the direct blocks
    new_file->i_size = 0;
    //working with direct pointers


    for (int i = 0; i < blocks_used; i++){

        if ((i < 12) && ((read_chk = fread(buf, 1, EXT2_BLOCK_SIZE, source_file)) > 0)) {
            //rintf("%s\n", buf);
        
        
            //allocate resources
            int block_num = find_free_block_index(bbm, sb->s_blocks_count);
            
            new_file->i_block[i] = block_num;
            
            //update the gd
            gd->bg_free_blocks_count--;
            sb->s_free_blocks_count --;
        
            unsigned char *blocks = disk + (EXT2_BLOCK_SIZE * block_num);
        
            memcpy(blocks, buf, read_chk);
    
            new_file->i_size += read_chk;
            
        }
    
        if(i == 12){
            int block_num = find_free_block_index(bbm, sb->s_blocks_count);
            
            new_file->i_block[i] = block_num;

            //update the gd
            gd->bg_free_blocks_count--;
            sb->s_free_blocks_count --;
            new_file->i_blocks += 2;
            continue;
            
        }

        if((i > 12) && ((read_chk = fread(buf, 1, EXT2_BLOCK_SIZE, source_file)) > 0)){
            

            unsigned int *indirect = (unsigned int *)(disk + (new_file->i_block)[12]*EXT2_BLOCK_SIZE);
            
            //allocate resources
            int block_num = find_free_block_index(bbm, sb->s_blocks_count);
           
            indirect[i-13] = block_num;
            
            //update the gd
            gd->bg_free_blocks_count--;
            sb->s_free_blocks_count --;

            unsigned char *blocks = disk + (EXT2_BLOCK_SIZE * block_num);
        
            memcpy(blocks, buf, read_chk);

            new_file->i_size += read_chk;
           
        }
        read_chk = 0;
    }
    new_file->i_blocks += (blocks_used-1)*2;
    new_file->i_links_count++;
    fclose(source_file);
    
    return 0;
        
    
}
