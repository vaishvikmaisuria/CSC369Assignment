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
    char *path2 = malloc(strlen(argv[2]) + 1); 
	strcpy(path2, argv[2]);

    printf("%s\n", path);
   
    if(path_exists(disk,path)){
		
        printf("%s\n", path2);
        LL *list = file_path_list(path2);
        printf("%d\n",list->last);
        struct ext2_dir_entry *dir_entry = get_struct(disk, inode_table, EXT2_ROOT_INO, list);
        if(dir_entry != NULL){
            printf("Last entry name: %s\n", dir_entry->name);
        }
        else{
            printf("Entry not reachable\n");
        }
        free_LL(list);
    }
    else{
        printf("Path is not valid\n");
    }
	free(path2);
    return 0;
	
}
