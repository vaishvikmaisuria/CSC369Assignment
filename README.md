# CSC369-OperatingSystems-Assignments
My assignments for CSC369 Operating Systems, University of Toronto

* Assignment 1 - Hijacking System Calls and Monitoring Processes
  * Wrote and Installed a basic kernel module to the Linux Kernel to hijack (intercept) System calls.
  
* Assignment 2 - Synchronization
  * Working with synchronization primitives to implement a traffic monitoring and guidance system that coordinates cars going through an intersection, by enforcing ordering between car arrivals in the same direction, avoiding potential crashes, and avoiding deadlocks. 
  * there are four entrances/exits to the intersection, each corresponding to a cardinal direction: North(n), South(S), East(E) and West(W). Each entrance into the intersection is referred to as a lane and is represented by a fixed sized buffer. Exits from the intersection are represented by a simple linked list for each exit direction. The intersection itself is broken into quadrants, each represented by a lock. 

* Assignment 3 - Virtual Memory (Page Tables and Replacement Algorithms)
  * To simulate the operation of page tables and page replacement. As I keep saying: the way to gain a solid understanding of the theory is by applying the concept in practice. Based on a virtual memory simulator, the first task is to implement virtual-to-physical address translation and demand paging using a two-level page table. The second task is to implement four different page replacement algorithms: FIFO, Clock, exact LRU, and OPT. 
  
* Assignment 4 - EXT 2 File Systems
  * Implementation of EXT 2 file system and write tools to modify ext2-format virtual disks. A set of programs in C that operate on an ext2 formatted virtual disk. The Executables:
    * ext2_cp: the program should work like cp, copying the file on your native file system onto the specified location on the disk
    * ext2_mkdir: the program creates a directory on the specified path on the disk.
    * ext2_ln: the program creates a link from the first specified file to the second specified path 
    * ext2_rm: the program removes the specified file from the disk
    * ext2_restore: the program is opposite of ext2_rm, and therefore restores the specified file that has been previously removed
    * ext2_checker: the program takes the name of an ext2 formatted virtual disk. The program a lightweight file system checker, which detects a small subset of possible file system inconsistencies and takes appropriate actions to fix them. 