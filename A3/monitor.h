extern int set_priority(int id, int *valid_cosigs);
extern int max_excluding(int id1, int id2);
extern void check_write_status(int id);
extern int get_car_count(int id);
extern int check_car_arrival(int id, int status);
extern void handle_signal(int id, int is_priority);
