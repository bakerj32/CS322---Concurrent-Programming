#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>   //setitimer

int cnt, m, n, thread_count;
pthread_mutex_t L;
n = 9999;
m = 9999;
thread_count = 2;
cnt = 0;

struct thread_data{
    int thread_id;
    int size;
    int end;
    int *ar;
    int *br;
    int *cr;
};


void * worker(void * threadarg){
    struct thread_data *my_data;
    my_data = (struct thread_data *) threadarg;
    int i, id, start, end, size;
    int *a;
    int *b;
    int *c;
    a = my_data->ar;
    b = my_data->br;
    c = my_data->cr;
    id = my_data->thread_id;
    size = my_data->size;
    start = id * (size/thread_count);
    end = (id+1) * (size/thread_count) + 1;
    // Do the same thing we did in the sequential section but do it in paralell this time. 
    for(i=start; i < end; i++){
        pthread_mutex_lock(&L);
             c[a[i]]++;
             c[b[i]]++;
             if((c[a[i]] == 2 && c[b[i]] == 2) && c[a[i]] == c[b[i]]) {cnt++; }
             else if (c[a[i]] == 2) { cnt++; }
             else if (c[b[i]] == 2) { cnt++; }
        pthread_mutex_unlock(&L);
    }
    // This section prepares the boundaries for the longer array.
    // and iterates through the array doing what we did above.
    start = id + size;
    end = my_data->end;
    if (n > m){
        for(i = start; i < end; i += thread_count){
            c[a[i]]++;
            if(c[a[i]] == 2){ 
                pthread_mutex_lock(&L);
                cnt++; 
                pthread_mutex_unlock(&L);
            }
        }
    }
    else if (m > n){
        for(i = start; i < end; i += thread_count){
            c[b[i]]++;
            if(c[b[i]] == 2){ 
                pthread_mutex_lock(&L);
                cnt++; 
                pthread_mutex_unlock(&L);
            }
        }
    }

}


main(int argc, char **argv) {
    int i, max, max_size, overflow_start, overflow_end, small_array_size;
    pthread_t *threads;
    struct timeval start, stop;
    double elapsed;
    int a[10000];
    int b[10000];
    for(i=0;i<n;i++){
        a[i] = i;
        b[i] = i;
    }
    // Determine which array is longer.
    if(n > m){
	    overflow_start = m + 1;
        overflow_end = n + 1;
        small_array_size = m + 1;
    }
    else {
        overflow_start = n + 1;
        overflow_end = m + 1;
        small_array_size = n + 1;
    };
    // Find the largest value in the arrays.
    if (a[n] >= b[m]){ max_size = a[n] + 1; }
    else {max_size = b[m] + 1;}
    // Initialize our array of size max(a[i], b[i]) and set every element to 0.
    int c[max_size];
    for(i=0; i < max_size; i++){
        c[i] = 0;
    }
    ////////////////////////////
    // Start Sequential section
    ////////////////////////////
    cnt = 0;
    gettimeofday(&start, NULL);
    // Increment a counter for in c[a[i]] and c[b[i]] to keep track of the number of
    // times a[i] or b[i] has occured. If the count is 2, then the value was in both arrays.
    for(i = 0; i < small_array_size; i++){
        c[a[i]]++;
        c[b[i]]++;
        if((c[a[i]] == 2 && c[b[i]] == 2) && c[a[i]] == c[b[i]]){ cnt++; }
        else if(c[a[i]] == 2){ cnt++; }
        else if(c[b[i]] == 2) {cnt++; }
    }
    // Do the same for the remaining portion of the longer array.
    if (n > m){
        for(i = overflow_start; i < overflow_end; i++){
            c[a[i]]++;
            if(c[a[i]] == 2){ cnt++; }
        }
    }
    else if (m > n){
        for(i = overflow_start; i < overflow_end; i++){
            c[b[i]]++;
            if(c[b[i]] == 2){ cnt++; }
        }
    }
    printf("Total Count: %d\n", cnt);
    gettimeofday(&stop, NULL);
    elapsed = ((stop.tv_sec - start.tv_sec) * 1000000+(stop.tv_usec-start.tv_usec))/1000000.0;
    printf("Time taken: %f\n", elapsed);

    /////////////////////////////////////////////////////
    // End sequential Section
    // Start concurrent section
    /////////////////////////////////////////////////

    // Reset our counter and our bucket array.
    cnt = 0;
    for(i=0; i < max_size; i++){
        c[i] = 0;
    }
    struct thread_data thread_data_array[thread_count];
    for(i=0; i < thread_count; i++){
        thread_data_array[i].thread_id = i;
        thread_data_array[i].ar = a;
        thread_data_array[i].br = b;
        thread_data_array[i].cr = c;
        thread_data_array[i].size = overflow_start - 1;
        thread_data_array[i].end = overflow_end;
    }
    threads = (pthread_t *) malloc(thread_count * sizeof(pthread_t));
    gettimeofday(&start, NULL);
    
    for(i=0; i < thread_count; i++){
        pthread_create(&threads[i], NULL, worker, (void *) &thread_data_array[i]);
    }

    for(i=0; i < thread_count; i++){
        pthread_join(threads[i], NULL);
    }
    printf("Total Count: %d\n", cnt);
    gettimeofday(&stop, NULL);
    elapsed = ((stop.tv_sec - start.tv_sec) * 1000000+(stop.tv_usec-start.tv_usec))/1000000.0;
    printf("Time taken: %f\n", elapsed);
    printf("# Threads: %d\n", thread_count);
    return 0;
}

