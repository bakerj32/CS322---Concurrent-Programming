#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>   //setitimer
#include "monitor.h"
#define LINELEN	1024
#define SIGNAL_COUNT 8
#define THREAD_COUNT 9
#define COSIG_COUNT 3

int set_priority(int id, int *valid_cosigs);
int max_excluding(int id1, int id2);
void check_write_status(int id);
int get_car_count(int id);
void increment_car_count(int id, int how_much);
void increment_ticks();

// Struct to hold relvant data for each individual process excluding the dispatcher.
struct Signal{
	int id;
	int priority;
	//ID's of signals that are allowed to be green while this signal is green.
	int valid_cosigs[3];

};



int done = 0;


//Input and output file names.
char *file_name = "smalldata.txt";

// Given a file name, this function will return the number of lines in that file.
int get_line_count(char *file_name){
	int count = 0;
	FILE *fp = fopen(file_name, "r");
	if(fp == NULL){ return 1; }
	char line[LINELEN];
	while(fgets(line, LINELEN, fp)){ count++; }
	fclose(fp);
	return count;
}

// Given a file name, this function will read in a file and store
// it's contents into the array it is passed.
int read_file(char *file_name, char ***datap){
	int i, j;
	char line[LINELEN];
	char **data;
	// Get line count and open file
	int line_count = get_line_count(file_name);
	FILE *fp = fopen(file_name, "r");
	if(fp == NULL){ printf("Error reading file.\n"); }

	// Allocate memory for 2d array
	data = (char **) malloc(line_count * sizeof(char *));
	for(i = 0; i < line_count; i++){ data[i] = (char *) malloc(SIGNAL_COUNT * sizeof(char)); }

	// Read file into array
	i = 0;
	while(fgets(line, LINELEN, fp)){
		// Increment by 2 to skip over tab delimiter
		for(j = 0; j < 15; j += 2){ data[i][j/2] = line[j]; }
		i++;
	}
	*datap = data;
	return line_count;
}



// This function is used to initialize an array of signal structures.
// The main purpose for this function is intialize each signal with a list
// of ID's of signals that are allowed to be green while the current signal is green.
void init_signals(struct Signal *Signals){

	Signals[0].id = 0;
	Signals[0].priority = 0;
	Signals[0].valid_cosigs[0] = 1;
	Signals[0].valid_cosigs[1] = 2;
	Signals[0].valid_cosigs[2] = 7;

	Signals[1].id = 1;
	Signals[1].priority = 0;
	Signals[1].valid_cosigs[0] = 0;
	Signals[1].valid_cosigs[1] = 3;
	Signals[1].valid_cosigs[2] = 4;

	Signals[2].id = 2;
	Signals[2].priority = 0;
	Signals[2].valid_cosigs[0] = 0;
	Signals[2].valid_cosigs[1] = 3;
	Signals[2].valid_cosigs[2] = 5;

	Signals[3].id = 3;
	Signals[3].priority = 0;
	Signals[3].valid_cosigs[0] = 2;
	Signals[3].valid_cosigs[1] = 1;
	Signals[3].valid_cosigs[2] = 6;

	Signals[4].id = 4;
	Signals[4].priority = 0;
	Signals[4].valid_cosigs[0] = 5;
	Signals[4].valid_cosigs[1] = 6;
	Signals[4].valid_cosigs[2] = 1;

	Signals[5].id = 5;
	Signals[5].priority = 0;
	Signals[5].valid_cosigs[0] = 4;
	Signals[5].valid_cosigs[1] = 7;
	Signals[5].valid_cosigs[2] = 2;

	Signals[6].id = 6;
	Signals[6].priority = 0;
	Signals[6].valid_cosigs[0] = 3;
	Signals[6].valid_cosigs[1] = 4;
	Signals[6].valid_cosigs[2] = 7;

	Signals[7].id = 7;
	Signals[7].priority = 0;
	Signals[7].valid_cosigs[0] = 0;
	Signals[7].valid_cosigs[1] = 5;
	Signals[7].valid_cosigs[2] = 6;
}





void *signal_worker(void *sig_data){
	int i, is_priority;
	struct Signal *signal_data;
	signal_data = (struct Signal *) sig_data;
	int id = signal_data->id;
	int status = get_car_count(id);
	while(done == 0){
		status = check_car_arrival(id, status);
		// Check to see if this signal should be the next to turn green.
		is_priority = set_priority(id, signal_data->valid_cosigs);
		handle_signal(id, is_priority);
		check_write_status(id);
		
	}
}

void *dispatcher(){
	char **data;
	int line_count, i, j;
	line_count = read_file(file_name, &data);
	
	// Create an intialize an array of structs for each signal.
	struct Signal Signals[SIGNAL_COUNT];
	init_signals(Signals);

	pthread_t *threads;
	threads = (pthread_t *) malloc(SIGNAL_COUNT * sizeof(pthread_t));
	// Intialize thread data and create threads.
	for(i = 0; i < SIGNAL_COUNT; i++){
		pthread_create(&threads[i], NULL, signal_worker, (void *) &Signals[i]);
	}
	
	// Read data from array and update sensor status.
	for(i = 0; i < line_count; i++){
		for(j = 0; j < SIGNAL_COUNT; j++){
			// Update car counts when a car arrives.			
			if(data[i][j] == '1'){
				increment_car_count(j, 1);
			}
		}
		// Increment the tick variable. This is equivalent to 200ms time passing.
		increment_ticks();
	}
	printf("Joining threads...\n");
	done = 1;

	for(i = 0; i < SIGNAL_COUNT; i++){
		pthread_join(threads[i], NULL);
	}
}

main(int argc, char **argv) {
	pthread_t worker_thread;
	pthread_create(&worker_thread, NULL, dispatcher, NULL);
	pthread_join(worker_thread, NULL);
}