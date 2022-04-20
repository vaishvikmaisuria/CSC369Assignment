#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "traffic.h"

extern struct intersection isection;

void sort_increasing_order(int *unsorted_lst, int n);
void add_car_to_lane(struct lane *l);
void pass_car_through_quad(struct lane *l,int *list,int index, struct car *lane_car);

/**
 * Populate the car lists by parsing a file where each line has
 * the following structure:
 *
 * <id> <in_direction> <out_direction>
 *
 * Each car is added to the list that corresponds with
 * its in_direction
 *
 * Note: this also updates 'inc' on each of the lanes
 */
void parse_schedule(char *file_name) {
    int id;
    struct car *cur_car;
    struct lane *cur_lane;
    enum direction in_dir, out_dir;
    FILE *f = fopen(file_name, "r");
    
    /* parse file */
    while (fscanf(f, "%d %d %d", &id, (int*)&in_dir, (int*)&out_dir) == 3) {
        
        /* construct car */
        cur_car = malloc(sizeof(struct car));
        cur_car->id = id;
        cur_car->in_dir = in_dir;
        cur_car->out_dir = out_dir;
        
        /* append new car to head of corresponding list */
        cur_lane = &isection.lanes[in_dir];
        cur_car->next = cur_lane->in_cars;
        cur_lane->in_cars = cur_car;
        cur_lane->inc++;
    }
    fclose(f);
}

/**
 * TODO: Fill in this function
 *
 * Do all of the work required to prepare the intersection
 * before any cars start coming
 *
 */
void init_intersection() {
    
    for (int i = 0; i < 4; i++) {
        /* synchronization */
        /* init for the four quadrants of the intersection. (lock) */
        pthread_mutex_init(&isection.quad[i], NULL);                        //isection.quad[i] = PTHREAD_MUTEX_INITIALIZER;
        /* init for the four lanes of the intersection. (lock) */
        pthread_mutex_init(&isection.lanes[i].lock, NULL);                  //isection.lanes[i].lock = PTHREAD_MUTEX_INITIALIZER;
        /* init for the producer(signal to add car to lanes or wait to add) for the four lanes of the intersection. (lock) */
        pthread_cond_init(&isection.lanes[i].producer_cv, NULL);            //isection.lanes[i].producer_cv = PTHREAD_COND_INITIALIZER;
        /* init for the Consumer(signal to pass car through intersection or wait for car) for the four lanes. (lock) */
        pthread_cond_init(&isection.lanes[i].consumer_cv, NULL);            //isection.lanes[i].consumer_cv = PTHREAD_COND_INITIALIZER;
        
        /* init for list of cars that are pending to pass through this lane. */
        isection.lanes[i].in_cars = NULL;
        
        /* init for list of cars that have passed the intersection into this lane. */
        isection.lanes[i].out_cars = NULL;
        
        /* init for number of cars passing through this lane. */
        isection.lanes[i].inc = 0;
        
        /* init for number of cars that have passed through this lane. */
        isection.lanes[i].passed = 0;
        
        /* malloc init for circular buffer implementation. */
        isection.lanes[i].buffer = malloc(sizeof(struct car*)*LANE_LENGTH);
        
        /* malloc init for circular buffer implementation for each car position in the bufffer. */
        for(int j = 0; j < LANE_LENGTH; j++){
            isection.lanes[i].buffer[j] = malloc(sizeof(struct car));
        }
        
        /* init for index of the first element element in the list. */
        isection.lanes[i].head =0;
        
        /* init for index of the last element in the list */
        isection.lanes[i].tail =0;
        
        /* init for maximum number of elements in the list */
        isection.lanes[i].capacity=LANE_LENGTH;
        
        /* number of elements currently in the list */
        isection.lanes[i].in_buf =0;
    }
}

/**
 * TODO: Fill in this function
 *
 * Populates the corresponding lane with cars as room becomes
 * available. Ensure to notify the cross thread as new cars are
 * added to the lane.
 *
 */
void *car_arrive(void *arg) {
    struct lane *l = arg;
    
    while(l->in_cars != NULL){
        //lock the lane.
        pthread_mutex_lock(&(l->lock));
        //advance a car into lane
        add_car_to_lane(l);
        //signal the intersection to accept a new car
        pthread_cond_broadcast(&(l->consumer_cv));
        //unlcock the lane lcok.
        pthread_mutex_unlock(&(l->lock));
    }
    return NULL;
}

void add_car_to_lane(struct lane *l){
    //check buffer for overflowing
    while(l->in_buf >= l->capacity){
        //if potential overflow wait for producer signal.
        pthread_cond_wait(&(l->producer_cv), &(l->lock));
    }
    /* advance car into lane */
    //malloc a new car temp variable to store in_car info.
    struct car *new_car = malloc(sizeof(struct car));
    //update new car with in_car info.
    new_car->id = l->in_cars->id;
    new_car->in_dir =  l->in_cars->in_dir;
    new_car->out_dir =  l->in_cars->out_dir;
    new_car->next = NULL;
    
    //the tail point to the last element which is the new car which is added to the buffer.
    l->buffer[l->tail] = new_car;
    
    //set the pending car as the car after the current one
    l->in_cars = l->in_cars->next;
    
    //increment the number of cars in the lane
    (l->in_buf)++;
    
    //update the tail to point to the last new element.
    (l->tail)++;
    
    // defualt case where the tail is greater than size of the buffer array,than we loop back the front(circular list).
    if(l->tail >= l->capacity){
        l->tail = 0;
    }
}

/**
 * TODO: Fill in this function
 *
 * Moves cars from a single lane across the intersection. Cars
 * crossing the intersection must abide the rules of the road
 * and cross along the correct path. Ensure to notify the
 * arrival thread as room becomes available in the lane.
 *
 * Note: After crossing the intersection the car should be added
 * to the out_cars list of the lane that corresponds to the car's
 * out_dir. Do not free the cars!
 *
 *
 * Note: For testing purposes, each car which gets to cross the
 * intersection should print the following three numbers on a
 * new line, separated by spaces:
 *  - the car's 'in' direction, 'out' direction, and id.
 *
 * You may add other print statements, but in the end, please
 * make sure to clear any prints other than the one specified above,
 * before submitting your final code.
 */

void *car_cross(void *arg) {
    
    struct lane *l = arg;
    
    // Run until the number of cars passed the quadrant is equal to cars passed the lane. (Base Case: if no cars in lane).
    while((l->passed != l->inc) && (l->inc != 0)){
        
        // lock the lane.
        pthread_mutex_lock(&(l->lock));
        
        // number of elements currently in the buffer is zero, than wait for the producer to add cars.
        while(l->in_buf == 0){
            pthread_cond_wait(&(l->consumer_cv), &(l->lock));
        }
        // Calculates the quadrants needed to make the turn.
        int *list = compute_path(l->buffer[l->head]->in_dir, l->buffer[l->head]->out_dir);
        int index = l->buffer[l->head]->out_dir;
        struct car *lane_car = l->buffer[l->head];
        
        
        // number of cars that have passed through this lane
        (l->passed)++;
        
        // number of elements currently in the list
        (l->in_buf)--;
        
        // circular buffer hence for each car crossed head is the next element.
        (l->head)= (l->head)+1;
        
        // defualt case where the head is greater or equal to the size of the buffer array,than set the head back the front (circular list).
        if(l->head >= l->capacity){
            l->head = 0;
        }
        
        // signal the lane to accept a new car.
        pthread_cond_broadcast(&(l->producer_cv));
        
        // unlock the lane.
        pthread_mutex_unlock(&(l->lock));
        
        
        pass_car_through_quad(l,list,index,lane_car);
        free(list);
    }
    return NULL;
}


void pass_car_through_quad(struct lane *l,int *list,int index, struct car *lane_car){
    // lock the quadrants used to cross the intersection by a car.
    for(int i =0; i < 4; i++){
        if(list[i] != -1){
            pthread_mutex_lock(&isection.quad[list[i]]);
        }
    }
    // locking the lane
    pthread_mutex_lock(&(isection.lanes[index].lock));
    
    // update the out_dir lanes to contain the cars that pass the intersection.
    lane_car->next = isection.lanes[index].out_cars;
    isection.lanes[index].out_cars = lane_car;
    
    //the car's 'in' direction, 'out' direction, and id * required print statment *
    printf("%d %d %d\n",lane_car->in_dir,lane_car->out_dir,lane_car->id);
    
    // unlocking the lane
    pthread_mutex_unlock(&(isection.lanes[index].lock));

    // unlock the quadrants used to cross the intersection by a car after the crossing.
    for(int i =0; i < 4; i++){
        if(list[i] != -1){
            pthread_mutex_unlock(&isection.quad[list[i]]);
        }
    }
    
    
}

/**
 * TODO: Fill in this function
 *
 * Given a car's in_dir and out_dir return a sorted
 * list of the quadrants the car will pass through.
 *
 //-----------------------------------------------------------------------------------------------------------------------------
 //       Car enters q1                  Car enters q2                      Car enters q3                      Car enters q4
 
 //from q1 to q1; right turn        //from q2 to q1; u-turn            //from q3 to q1; left-turn        //from q4 to q1; straight
 //from q1 to q2; straight        //from q2 to q2; right-turn        //from q3 to q2; u-turn            //from q4 to q2; left-turn
 //from q1 to q3; left turn        //from q2 to q3; straight        //from q3 to q3; right-turn        //from q4 to q3; u-turn
 //from q1 to q4; u-turn            //from q2 to q4; left-turn        //from q3 to q4; straight        //from q4 to q4; right-turn
 //-----------------------------------------------------------------------------------------------------------------------------
 
 //-----------------------------------------------------------------------------------------------------------------------------
 //        Straight                        Left Turn                        Right Turn                          U-Turn
 
 //from 1 to 2; straight        //from 1 to 3; left turn        //from 1 to 1; right turn        //from 1 to 4; u-turn
 //from 2 to 3; straight        //from 2 to 4; left-turn        //from 2 to 2; right-turn        //from 2 to 1; u-turn
 //from 3 to 4; straight        //from 3 to 1; left-turn        //from 3 to 3; right-turn        //from 3 to 2; u-turn
 //from 4 to 1; straight        //from 4 to 2; left-turn        //from 4 to 4; right-turn        //from 4 to 3; u-turn
 
 //    rule: out = in + 1        //rule: mod(out) = mod(in)            //rule: out = in               //rule: in = out + 1
 //-----------------------------------------------------------------------------------------------------------------------------
 *
 */
int *compute_path(enum direction in_dir, enum direction out_dir) {
    
    // defualt values if the quadrants are not used.
    int in = -1;
    int out = -1;
    
    // for in_dir to in : 0 = 1, 1 = 2, 2 = 3, 3 = 0 for math calculations
    in = (in_dir+1)%4;
    // out = out_dir
    out = out_dir;
    
    // malloc space on heap for list
    int *list = malloc(sizeof(int)*4);
    
    // initially the values are set to 10
    for(int i = 0; i < 4; i++){
        list[i] = 10;
    }
    
    //check for right turn
    if(in == out){
        list[0] = in;
    }
    
    //check for straight
    else if((out%4) == (in+1)%4){
        list[0] = in;
        list[1] = out;
    }
    
    //check for left turn
    else if(in%2 == out%2){
        for(int i = 0; i<3; i++){
            list[i] = (in + i)%4;
        }
    }
    
    //check for u-turn
    else if((in%4) == (out+1)%4){
        for(int i =0; i<4; i++){
            list[i] = (in + i)%4;
        }
    }
    //sort list by increasing order
    sort_increasing_order(list, 4);
    
    return list;
}
/**
 * Used to sort the list computed by the function compute path
 * so the quadrants are ordered in increasing order.
 */
void sort_increasing_order(int *unsorted_lst, int n){
    int i;
    int j;
    int min_index;
    for (i = 0; i < n-1; i++) {
        min_index = i;
        for (j = i+1; j < n; j++){
            if (unsorted_lst[j] < unsorted_lst[min_index]){
                min_index = j;
            }
        }
        int swap = unsorted_lst[i];
        unsorted_lst[i] = unsorted_lst[min_index];
        unsorted_lst[min_index] = swap;
    }
    for (int s = 0; s < n; s++) {
        if(unsorted_lst[s] == 10){
            unsorted_lst[s] = -1;
        }
    }
}
