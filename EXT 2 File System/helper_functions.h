#ifndef _helper_functions_h
#define _helper_functions_h

typedef struct LL {
    char name[EXT2_NAME_LEN];
	int last; 	//1 is last 0 is not last
    struct LL *next;
} LL; 

void free_LL(LL *list);

char directory_file_type(struct ext2_dir_entry *entry);
int valid_inode(unsigned char block, int index);
char file_mode(unsigned short i_mode);
unsigned char make_inode_valid(unsigned char block, int index);
int check_name_len (char * name);
char *file_path_name(char *path);
struct LL *file_path_list(char *path);
int path_exists(unsigned char *disk, char *path);
int recursive_path(struct ext2_inode *inode_table, int index, struct LL *list,unsigned char *disk);

int find_valid_indode_index(unsigned char *ibm, int first_inode, int inode_count);
int find_invalid_indode_index(unsigned char *ibm, int first_inode, int inode_count);
int find_free_block_index(unsigned char *bbm, int block_count);

void add_initial_directories(unsigned char *dir_address, int index, int parent);

int pad_rec_len(int rec_length);

int new_directory_fits(struct ext2_dir_entry *last_directory,int counter,char* name);

int remove_with_name(struct ext2_inode *inode_table, int index, struct LL *list);

unsigned char make_inode_valid(unsigned char block, int index);
unsigned char make_inode_invalid(unsigned char block, int index);

struct ext2_dir_entry* get_struct(unsigned char *disk, struct ext2_inode *inode_table, int index, LL *list);

void set_default_inode_info(struct ext2_inode *inode_table, int offset);

char *get_target_file_name (char *target_path);
int pad_file_len(int fsize);

int check_all_inodes_invalid(struct ext2_inode *inode, int offset, unsigned char *disk, unsigned char *bbm, unsigned int *indirect);
#endif
