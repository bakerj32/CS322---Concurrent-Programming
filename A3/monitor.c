#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>   //setitimer
#include "monitor.h"

#define LINELEN	1024
#define SIGNAL_COUNT 8
#define THREAD_COUNT 9
#define COSIG_COUNT 3

int ticks = 0;
int light_on = 0;
int next_light = 0;

int car_total = 0;

int idle_count[SIGNAL_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0};
int car_count[SIGNAL_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0};
int sig_status[SIGNAL_COUNT] = {1, 1, 1, 1, 1, 1, 1, 1};
int write_status[2][6] = {{-1, -1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1, -1}};

char *aliases[SIGNAL_COUNT] = {"S", "LS", "N", "LN", "E", "LE", "W", "LW"};

// This variable will hold the number of cars waiting at the signal with the most cars plus it's
// max valid co-signal.
int priority_cnt = 0;
// This will hold the id's of the above 2 signals
int priority_sigs[2];
pthread_mutex_t priority_mutex;
pthread_mutex_t light_mutex;
pthread_mutex_t tick_mutex;
pthread_mutex_t write_mutex;

int light_status[SIGNAL_COUNT];

// Intialize an array of mutexes for each signal.
pthread_mutex_t mutex[SIGNAL_COUNT] = {
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
};

//Input and output file names.
char *outfile_name = "output.txt";

void increment_ticks(){
	pthread_mutex_lock(&tick_mutex);
	ticks += 1;
	pthread_mutex_unlock(&tick_mutex);
}

void increment_car_count(int id, int how_much){
	pthread_mutex_lock(&mutex[id]);
	car_count[id] += how_much;
	pthread_mutex_unlock(&mutex[id]);

}

// Given a file name, this function will write data to the file specified.
void write_file(char *outfile_name, int *data){
	int rows, i;
	FILE *fp = fopen(outfile_name, "a");
	rows = 6;
	for(i = 0; i < rows; i++){
		fprintf(fp, "%d\t", data[i]);
	}
	fprintf(fp, "\n");
	fclose(fp);
}

int check_car_arrival(int id, int status){
	pthread_mutex_lock(&mutex[id]);
		// Check to see if a car has arrived.
		if(status != car_count[id]){
			// Prepare an array to write to the output file.
			int data[6];
			status = car_count[id];
			pthread_mutex_lock(&tick_mutex);
			data[0] = ticks * 200;
			pthread_mutex_unlock(&tick_mutex);
			data[1] = 1;
			data[2] = id;
			data[3] = 0;
			data[4] = 0;
			data[5] = 0;
			// Update output file signifying the arrival of a car if we haven't done so already.
			// Make sure we're writing when we are suppose to.
			pthread_mutex_lock(&tick_mutex);
			if(data[0] == (ticks * 200)){
				write_file(outfile_name, data);
			}
			pthread_mutex_unlock(&tick_mutex);
		}
	return status;
}

int get_car_count(int id){
	int count;
	pthread_mutex_lock(&mutex[id]);
	count = car_count[id];
	pthread_mutex_unlock(&mutex[id]);
	return count;
}

// This function will check to see if the current signal should write to the file when a signal is turning red.
// The purpose of this is to make sure that the "red" case is printed where it should be since it is actually
// computed earlier than when it should be printed.
void check_write_status(int id){
	int i;
	// Check if the time we are suppose to write is now and that we are a process that should be writing.
	pthread_mutex_lock(&write_mutex);
	if((write_status[0][0] == (ticks * 200) && write_status[0][2] == id) || (write_status[1][0] == (ticks * 200) && write_status[1][2] == id)){
		if(write_status[0][2] == id){
			// Write to file and reset the write_status array.
			write_file(outfile_name, write_status[0]);
			for(i = 0; i < 6; i++){ write_status[0][i] = -1; }
		}
		else{ 
			// Write to file and reset the write_status array.
			write_file(outfile_name, write_status[1]);
			for(i = 0; i < 6; i++){ write_status[1][i] = -1; }
		}
	}
	pthread_mutex_unlock(&write_mutex);
	pthread_mutex_unlock(&light_mutex);
	pthread_mutex_unlock(&mutex[id]);
}

// This function will return the number of cars waiting at the signal
// with the largest amount of cars waiting at it, excluding the the signals
// passed.
int max_excluding(int id1, int id2){
	int max, i;
	max = 0;
	for(i = 0; i < SIGNAL_COUNT; i++){
		if(i != id1 && i != id2){
			if(car_count[i] > max){
				max = car_count[i];
			}
		}
	}
	return max;
}


// This function, given an ID and signal struct, will determine if this current signal
// should be the next signal to turn green based on either a) how long it has been since
// this signal was green or b) the number of cars waiting at this signal and it's other co-signals
int set_priority(int id, int *valid_cosigs){
	int i, cars_waiting, max, max_id, max_idle, max_idle_id, cosig_id, is_priority;
	cars_waiting = car_count[id];
	is_priority = 0;
	max = 0;
	max_idle = 0;
	// Find the "most idle" signal, find how long it's been idle.
	for(i = 0; i < SIGNAL_COUNT; i++){
		if(idle_count[i] > max_idle){ 
			max_idle_id = i;
			max_idle = idle_count[i];
		}
	}
	// If the current signal is "most idle" and idle count above certain threshold, make priority.
	if(max_idle_id == id && max_idle > 5){
		for(i = 0; i < COSIG_COUNT; i++){
			//Find the co-signal with the greatest amount of cars waiting at it.
			cosig_id = valid_cosigs[i];
			if(car_count[cosig_id] > max){ 
				max = car_count[cosig_id]; 
				max_id = cosig_id;
			}

		}
		pthread_mutex_lock(&priority_mutex);
		priority_cnt = max + cars_waiting;
		// Set the priority signals to this current ID and its max co-sig
		priority_sigs[0] = id;
		priority_sigs[1] = cosig_id;
		pthread_mutex_unlock(&priority_mutex);
		is_priority = 1;
	}
	// Else compute priority by raw car counts.
	else{
		for(i = 0; i < COSIG_COUNT; i++){
			//Find the co-signal with the greatest amount of cars waiting at it.
			cosig_id = valid_cosigs[i];
			if(car_count[cosig_id] > max){ 
				max = car_count[cosig_id]; 
				max_id = cosig_id;
			}

		}
		
		// If this current signal's car count + the max cosignal car count is greater than that
		// of any other combination of lights, make this light (and its co-sig) the priority.
		if(max + cars_waiting > priority_cnt){
			pthread_mutex_lock(&priority_mutex);
			priority_cnt = max + cars_waiting;
			priority_sigs[0] = id;
			priority_sigs[1] = cosig_id;
			pthread_mutex_unlock(&priority_mutex);
			is_priority = 1;
		}
	}
	return is_priority;
}

void handle_signal(int id, int is_priority){
	int i, cars_passed1, cars_passed2, next_max;
	pthread_mutex_lock(&light_mutex);
		if(is_priority == 1 && ticks > next_light){
			// This variable will keep track of how long this light as been green for.
			int ticks_passed;
			// Ensure that this light is green for at least 600ms
			next_light = ticks + 2;
			pthread_mutex_lock(&priority_mutex);
			// Reset the idle variable for this light and it's cosig.
			idle_count[id] = 0;
			idle_count[priority_sigs[1]] = 0;
			// Turn lights green.
			sig_status[priority_sigs[0]] = 0;
			sig_status[priority_sigs[1]] = 0;
			pthread_mutex_unlock(&priority_mutex);

			// These data variables represent the columns that will be written to the output file.
			// This is case "2", or a light turning green.
			int data1[6];
			int data2[6];
			cars_passed1 = 0;
			cars_passed2 = 0;

			pthread_mutex_lock(&tick_mutex);
			data1[0] = ticks * 200;
			data2[0] = ticks * 200;
			pthread_mutex_unlock(&tick_mutex);
			data1[1] = 2;
			data2[1] = 2;
			pthread_mutex_lock(&priority_mutex);
			data1[2] = priority_sigs[0];
			data2[2] = priority_sigs[1];
			pthread_mutex_unlock(&priority_mutex);
			data1[3] = 0;
			data2[3] = 0;
			data1[4] = 0;
			data2[4] = 0;
			data1[5] = 0;
			data2[5] = 0;
			// Write the data to the file indicating the light as turned to green/
			write_file(outfile_name, data1);
			write_file(outfile_name, data2);

			// Find the light with most cars waiting for it excluding
			next_max = max_excluding(priority_sigs[0], priority_sigs[1]);
			ticks_passed = 0;
			pthread_mutex_lock(&priority_mutex);
			// While the combined car count of the two green lights is greater than the highest car count
			// of any other signal we will stay green.
			while(priority_cnt > next_max || (car_count[priority_sigs[0]] != 0) ){
				if(car_count[priority_sigs[0]] > 0){
					car_count[priority_sigs[0]]--;
					cars_passed1++;
					car_total--;

				}
				if(car_count[priority_sigs[1]] > 0){
					car_count[priority_sigs[1]]--;
					cars_passed2++;
					car_total--;
				}
				priority_cnt--;
				// Increment next light to indicate that we are kept the light green for another tick.
				next_light++;
				ticks_passed++;
			}

			// Increment the idle counters for every signal. (current signals set to 0 previously)
			for(i = 0; i < SIGNAL_COUNT; i++){
				idle_count[i]++;
			}
			pthread_mutex_unlock(&priority_mutex);

			// Prepare the write status for writing once the ticks catch up.
			// This is for the "3" case, or a light turning red.
			pthread_mutex_lock(&write_mutex);
			write_status[0][0] = (ticks + ticks_passed) * 200;
			write_status[0][1] = 3;
			write_status[0][2] = priority_sigs[0];
			write_status[0][3] = ticks_passed * 200;
			write_status[0][4] = cars_passed1;
			write_status[0][5] = car_count[priority_sigs[0]];
			write_status[1][0] = (ticks + ticks_passed) * 200;
			write_status[1][1] = 3;
			write_status[1][2] = priority_sigs[1];
			write_status[1][3] = ticks_passed * 200;
			write_status[1][4] = cars_passed2;
			write_status[1][5] = car_count[priority_sigs[1]];
			pthread_mutex_unlock(&write_mutex);
	
			// Set signals to red.
			sig_status[priority_sigs[0]] = 1;
			sig_status[priority_sigs[1]] = 1;
			pthread_mutex_unlock(&priority_mutex);
			light_on = 0;	
		}
}
