CFLAGS=-std=gnu99 -Wall -g

all: helper_functions ext2_mkdir ext2_rm ext2_rm_bonus ext2_ln readimage ext2_cp ext2_restore ext2_restore_bonus ext2_checker

helper_functions: helper_functions.c
	gcc $(CFLAGS) -g -c $<

ext2_testpointer: ext2_testpointer.o helper_functions.o
	gcc $(CFLAGS) -o ext2_testpointer $^

ext2_mkdir :  ext2_mkdir.o helper_functions.o
	gcc $(CFLAGS) -o ext2_mkdir $^
	
ext2_cp : ext2_cp.o helper_functions.o
	gcc $(CFLAGS) -o ext2_cp $^
	
ext2_ln : ext2_ln.o helper_functions.o
	gcc $(CFLAGS) -o ext2_ln $^
	
ext2_rm : ext2_rm.o helper_functions.o
	gcc $(CFLAGS) -o ext2_rm $^
	
ext2_rm_bonus : ext2_rm_bonus.o helper_functions.o
	gcc $(CFLAGS) -o ext2_rm_bonus $^

ext2_restore : ext2_restore.o helper_functions.o
	gcc $(CFLAGS) -o ext2_restore $^
	
ext2_restore_bonus : ext2_restore_bonus.o helper_functions.o
	gcc $(CFLAGS) -o ext2_restore_bonus $^
	
ext2_checker :  ext2_checker.o helper_functions.o
	gcc $(CFLAGS) -o ext2_checker $^

readimage :  readimage.o 
	gcc $(CFLAGS) -o readimage $^

%.o : %.c
	gcc $(CFLAGS) -g -c $<

clean : 
	rm -f *.o readimage ext2_mkdir ext2_cp ext2_ls ext2_ln ext2_rm ext2_rm_bonus ext2_restore ext2_restore_bonus ext2_checker *~
