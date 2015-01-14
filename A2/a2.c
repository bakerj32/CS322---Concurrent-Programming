#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>   //setitimer
#define LINELEN	1024
#define SIGNAL_COUNT 8
#define THREAD_COUNT 9
#define COSIG_COUNT 3

// Struct to hold relvant data for each individual process excluding the dispatcher.
struct Signal{
	int id;
	char *alias;
	int priority;
	//ID's of signals that are allowed to be green while this signal is green.
	int valid_cosigs[3];

};

int done = 0;
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
char *file_name = "largedata.txt";
char *outfile_name = "output.txt";

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

// Given a file name, this function will write data to the file specified.
void write_file(char *file_name, int *data){
	int rows, i;
	FILE *fp = fopen(file_name, "a");
	rows = 6;
	for(i = 0; i < rows; i++){
		fprintf(fp, "%d\t", data[i]);
	}
	fprintf(fp, "\n");
	fclose(fp);
}

// This function is used to initialize an array of signal structures.
// The main purpose for this function is intialize each signal with a list
// of ID's of signals that are allowed to be green while the current signal is green.
void init_signals(struct Signal *Signals){

	Signals[0].id = 0;
	Signals[0].alias = aliases[0];
	Signals[0].priority = 0;
	Signals[0].valid_cosigs[0] = 1;
	Signals[0].valid_cosigs[1] = 2;
	Signals[0].valid_cosigs[2] = 7;

	Signals[1].id = 1;
	Signals[1].alias = aliases[1];
	Signals[1].priority = 0;
	Signals[1].valid_cosigs[0] = 0;
	Signals[1].valid_cosigs[1] = 3;
	Signals[1].valid_cosigs[2] = 4;

	Signals[2].id = 2;
	Signals[2].alias = aliases[2];
	Signals[2].priority = 0;
	Signals[2].valid_cosigs[0] = 0;
	Signals[2].valid_cosigs[1] = 3;
	Signals[2].valid_cosigs[2] = 5;

	Signals[3].id = 3;
	Signals[3].alias = aliases[3];
	Signals[3].priority = 0;
	Signals[3].valid_cosigs[0] = 2;
	Signals[3].valid_cosigs[1] = 1;
	Signals[3].valid_cosigs[2] = 6;

	Signals[4].id = 4;
	Signals[4].alias = aliases[4];
	Signals[4].priority = 0;
	Signals[4].valid_cosigs[0] = 5;
	Signals[4].valid_cosigs[1] = 6;
	Signals[4].valid_cosigs[2] = 1;

	Signals[5].id = 5;
	Signals[5].alias = aliases[5];
	Signals[5].priority = 0;
	Signals[5].valid_cosigs[0] = 4;
	Signals[5].valid_cosigs[1] = 7;
	Signals[5].valid_cosigs[2] = 2;

	Signals[6].id = 6;
	Signals[6].alias = aliases[6];
	Signals[6].priority = 0;
	Signals[6].valid_cosigs[0] = 3;
	Signals[6].valid_cosigs[1] = 4;
	Signals[6].valid_cosigs[2] = 7;

	Signals[7].id = 7;
	Signals[7].alias = aliases[7];
	Signals[7].priority = 0;
	Signals[7].valid_cosigs[0] = 0;
	Signals[7].valid_cosigs[1] = 5;
	Signals[7].valid_cosigs[2] = 6;
}

// This function, given an ID and signal struct, will determine if this current signal
// should be the next signal to turn green based on either a) how long it has been since
// this signal was green or b) the number of cars waiting at this signal and it's other co-signals
int set_priority(int id, struct Signal *signal_data){
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
			cosig_id = signal_data->valid_cosigs[i];
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
			cosig_id = signal_data->valid_cosigs[i];
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

// This function will check to see if the current signal should write to the file when a signal is turning red.
// The purpose of this is to make sure that the "red" case is printed where it should be since it is actually
// computed earlier than when it should be printed.
void check_write_status(int id){
	int i;
	// Check if the time we are suppose to write is now and that we are a process that should be writing.
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
}

void *signal_worker(void *sig_data){
	int i, is_priority, next_max, cars_passed1, cars_passed2, last_write;
	struct Signal *signal_data;
	signal_data = (struct Signal *) sig_data;
	int id = signal_data->id;
	char *alias = signal_data->alias;
	int status;
	pthread_mutex_lock(&mutex[id]);
	status = car_count[id];
	pthread_mutex_unlock(&mutex[id]);
	while(done == 0){
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
		// Check to see if this signal should be the next to turn green.
		is_priority = set_priority(id, signal_data);
		// If we are suppose to turn green and no other lights are green:
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
		
		// Check to see if we need to write the signal turning red yet.
		pthread_mutex_lock(&write_mutex);
		check_write_status(id);
		pthread_mutex_unlock(&write_mutex);
		
		pthread_mutex_unlock(&light_mutex);
		pthread_mutex_unlock(&mutex[id]);
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
				pthread_mutex_lock(&mutex[j]);
				car_count[j] += 1;
				car_total++;
				pthread_mutex_unlock(&mutex[j]);
			}
		}
		pthread_mutex_lock(&tick_mutex);
		// Increment the tick variable. This is equivalent to 200ms time passing.
		ticks++;
		pthread_mutex_unlock(&tick_mutex);
		pthread_mutex_lock(&priority_mutex);
		pthread_mutex_unlock(&priority_mutex);
	}
	// We have processed every line. Join threads and finish.
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
