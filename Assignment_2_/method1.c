/* Created by: Muhammad Junaid Aslam
*  Teaching Assistant, Real-Time Systems Course IN4343, 2020
*  PhD Candidate Technical University of Delft, Netherlands.
*/

// Header File inclusion
#define _GNU_SOURCE
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>

// Global Defines
#define TRUE 1
#define FALSE 0

#define NUM_THREADS 4
#define HIGHEST_PRIORITY	99

#define handle_error_en(en, msg) \
               do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define POL_TO_STR(x) \
       x==SCHED_FIFO?"FIFO":x==SCHED_RR?"RR":x==SCHED_OTHER?"OTHER":"INVALID"

#define DEFAULT_BUSY_WAIT_TIME 6000 // 5 Milliseconds
#define DEFAULT_RR_LOOP_TIME 1000 // 1 Millisecond
#define MICRO_SECOND_MULTIPLIER 1000000
#if !defined(_POSIX_THREAD_CPUTIME)
	#error No thread time available for workload simulation
#endif

// Global data and structures
static int trace_fd = -1;
static int inputpolicy = 0;

struct thread_args {
	unsigned int thread_period;
	unsigned int thread_priority;
	unsigned int thread_policy;
	unsigned int thread_affinity; 
	unsigned int thread_number;
	struct timespec thread_start_time;
	struct timespec thread_end_time;
	unsigned long long thread_end_timestamp;
	double thread_exectime;
	long long thread_deadline;
	long long thread_response_time;
};

static struct thread_args results[NUM_THREADS];
typedef void* (*thread_func_prototype)(void*);

/*<======== Do not change anything in this function =========>*/
static void timespec_add_us(struct timespec *t, long us)
{
	// add microseconds to timespecs nanosecond counter
	t->tv_nsec += us*1000;
	// if wrapping nanosecond counter, increment second counter
	if (t->tv_nsec > 1000000000)
	{
		t->tv_nsec = t->tv_nsec - 1000000000;
		t->tv_sec += 1;
	}
}

/*<======== Do not change anything in this function =========>*/
static inline int timespec_cmp(struct timespec *now, struct timespec *next)
{
    int diff =  now->tv_sec - next->tv_sec;
    return diff ? diff : now->tv_nsec - next->tv_nsec;
}

/*<======== Do not change anything in this function =========>*/
static unsigned long long getTimeStampMicroSeconds(void)
{
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * 1000000 + currentTime.tv_usec;
}

/*<======== Do not change anything in this function =========>*/
static long clock_diff(struct timespec start, struct timespec end)
{
	/* It returns difference in time in nano-seconds */
    struct timespec temp;

    if ((end.tv_nsec-start.tv_nsec)<0)
    {
            temp.tv_sec = end.tv_sec-start.tv_sec-1;
            temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    }
    else 
    {
            temp.tv_sec = end.tv_sec-start.tv_sec;
            temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp.tv_nsec;
}

/*<======== Do not change anything in this function =========>*/
static int set_attributes(int *priorities, int num_thr, int *Affinities, int *Policies, pthread_attr_t *attributes)
{
	cpu_set_t cpuset;

	for(int i=0; i<num_thr; i++)
	{
		if((pthread_attr_init(&attributes[i])!=0)) {
			printf("%d - %s %d\n", __LINE__, "Error Initializing Attributes for Thread:", i+1);
			return FALSE;
		}

		struct sched_param param;

		param.sched_priority = priorities[i];

		CPU_ZERO(&cpuset);
		// CPU_SET: This macro adds cpu to the CPU set set.
		CPU_SET(Affinities[i], &cpuset);

		if((pthread_attr_setschedpolicy(&attributes[i], Policies[i])!=0)) 
		{
			printf("%s %d\n", "Error Setting Scheduling Policy for Thread:", i);
			return FALSE;
		}

		if((pthread_attr_setschedparam(&attributes[i], &param) != 0)) 
		{
			printf("%s %d\n", "Error Setting Scheduling Parameters for Thread:", i);
			return FALSE;
		} 

		if((pthread_attr_setaffinity_np(&attributes[i], sizeof(cpu_set_t), &cpuset)!=0))
		{
			printf("%s %d\n", "Error Setting CPU Affinities for Thread:", i);
			return FALSE;
		}

		if((pthread_attr_setinheritsched(&attributes[i], PTHREAD_EXPLICIT_SCHED)!=0))
		{
			printf("%s %d\n", "Error Setting CPU Attributes for Thread:", i);
			return FALSE;
		}	
	}

	return TRUE;
}

/*<======== Do not change anything in this function =========>*/
static void trace_write(const char *fmt, ...)
{
        va_list ap;
        char buf[256];
        int n;

        if (trace_fd < 0){return;}

        va_start(ap, fmt);
        n = vsnprintf(buf, 256, fmt, ap);
        va_end(ap);

        write(trace_fd, buf, n);
}

/*<======== Do not change anything in this function =========>*/
// static void workload(int tid)
// {
// 	struct timeval lvTimeVal;
// 	if(gettimeofday(&lvTimeVal, NULL) != 0){
// 		fprintf(stderr, "%s\n", "Error Fetching Clock Start Time."); return;
// 	}
// 	struct timeval lvTimer = lvTimeVal, lvTimer2 = lvTimeVal;
// 	unsigned int counter = 1;

// 	while(1)
// 	{
// 		if(gettimeofday(&lvTimeVal, NULL) != 0){
// 			fprintf(stderr, "%s\n", "Error Fetching Clock Start Time."); return;
// 		}

// 		if  (
// 				(
// 					(lvTimeVal.tv_sec*MICRO_SECOND_MULTIPLIER+lvTimeVal.tv_usec) - 
// 					(lvTimer.tv_sec*MICRO_SECOND_MULTIPLIER+lvTimer.tv_usec)
// 				) 
// 					>= DEFAULT_BUSY_WAIT_TIME
// 			) {break;}

// 		if  (
// 				(inputpolicy == SCHED_RR) && 
// 				(
// 					(
// 						(lvTimeVal.tv_sec*MICRO_SECOND_MULTIPLIER+lvTimeVal.tv_usec) - 
// 						(lvTimer2.tv_sec*MICRO_SECOND_MULTIPLIER+lvTimer2.tv_usec)
// 					)
// 					 	>= DEFAULT_RR_LOOP_TIME
// 				)
// 			) 
// 		{
// 			lvTimer2.tv_sec = lvTimeVal.tv_sec;
// 			lvTimer2.tv_usec = lvTimeVal.tv_usec;
// 			trace_write("RTS_Thread_%d Loop ... %d", tid, counter++);
// 		}
// 	}
// }
static void workload(int tid)
{


	struct timespec lvTimeVal;
	if(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &lvTimeVal) != 0){
		fprintf(stderr, "%s\n", "Error Fetching Clock Start Time."); return;
	}
	struct timespec lvTimer = lvTimeVal, lvTimer2 = lvTimeVal;
	unsigned int counter = 1;


	while(1)
	{
		if(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &lvTimeVal) != 0){
			fprintf(stderr, "%s\n", "Error Fetching Clock Start Time."); return;
		}

		if  (
				(
					(lvTimeVal.tv_sec*MICRO_SECOND_MULTIPLIER+lvTimeVal.tv_nsec/1000) - 
					(lvTimer.tv_sec*MICRO_SECOND_MULTIPLIER+lvTimer.tv_nsec/1000)
				) 
					>= DEFAULT_BUSY_WAIT_TIME
			) {break;}

		if  (
				(inputpolicy == SCHED_RR) && 
				(
					(
						(lvTimeVal.tv_sec*MICRO_SECOND_MULTIPLIER+lvTimeVal.tv_nsec/1000) - 
						(lvTimer2.tv_sec*MICRO_SECOND_MULTIPLIER+lvTimer2.tv_nsec/1000)
					)
					 	>= DEFAULT_RR_LOOP_TIME
				)
			) 
		{
			lvTimer2.tv_sec = lvTimeVal.tv_sec;
			lvTimer2.tv_nsec = lvTimeVal.tv_nsec;
			trace_write("RTS_Thread_%d Loop ... %d", tid, counter++);
		}
	}
}


/*<======== This is where you have to work =========>*/
static void* Thread(void *inArgs)
{
	struct thread_args *args = (struct thread_args*)inArgs;
	int tid = args->thread_number;
	struct timespec interval;

	interval.tv_sec=0;
	interval.tv_nsec = 5e6;
	for (int i = 0; i < 3; i++) {
				
		nanosleep(&interval, 0);

		trace_write("RTS_Thread_%d Priority:%d\n", 
		args->thread_number,  
		args->thread_priority);

		trace_write("RTS_Thread_%d Terminated", args->thread_number);
	}

}

/*<======== Do not change anything in this function =========>*/
static void help(void) 
{	
	printf("Error... Provide One of the Following Scheduling Policies\n");
	printf("1 - FIFO\n");
	printf("2 - RR\n");
	printf("4 - OTHER\n");
}

/*<======== Do not change anything in this function =========>*/
static int comparator(const void* p, const void* q) 
{ 
   if (((struct thread_args*)p)->thread_end_timestamp < ((struct thread_args*)q)->thread_end_timestamp) {return -1;}
   else if (((struct thread_args*)p)->thread_end_timestamp > ((struct thread_args*)q)->thread_end_timestamp) {return 1;}
   else {return 0;}
} 

int main(int argc, char **argv)
{
	/*<======== You have to change the following two data structures whose values will be assigned to threads in sequence =========>*/    
	/* NOTE: Do not forget to change the value of macro NUM_THREADS when you want to change the number of threads, 
	 * and then fill the following arrays accordingly. Default value of NUM_THREADS is 4. 
	 * -------------------------------------------------------------------------------------------------------------------------------
	 * Warning: If you run the code without filling in priorities, you will get runtime stack error.
	 * If you do not have a multicore computer, then change the value of affinities[i] from 1 to 0.
	 */

	int priorities[NUM_THREADS]= {4, 1, 2, 3}; /* Priorities are in the range from 1 to 99 when it comes to SCHED_FIFO and SCHED_RR, and for  
	SCHED_OTHER, the priority is always 0 */

	int periods[NUM_THREADS] = {50e3, 50e3, 50e3, 50e3} ;	// Used in calculation of next period value in a periodic task / thread.

	/*<======== Do not change anything below unless you have to change value of affinities[i] below =========>*/
	
	thread_func_prototype thread_functions[NUM_THREADS] = {Thread};
	pthread_t thread_id[NUM_THREADS];
	pthread_attr_t attributes[NUM_THREADS];
	int affinities[NUM_THREADS];
	int policies[NUM_THREADS];
	struct thread_args lvargs[NUM_THREADS] = {0};
	srand((unsigned) time(NULL));

	if(argc <= 1)
	{
		help();
		return FALSE; 
	} else {
		if(strcmp(argv[1], "RR") == 0) {
			printf("Round Robin Scheduling\n");
			for(int i = 0; i < NUM_THREADS; i++) {
				policies[i] = SCHED_RR;
				inputpolicy = SCHED_RR;
				affinities[i]	=	1;
			}
		} else if(strcmp(argv[1], "FIFO") == 0) {
			printf("First in First out Scheduling\n");
			for(int i = 0; i < NUM_THREADS; i++) {
				policies[i] = SCHED_FIFO;
				inputpolicy = SCHED_FIFO;
				affinities[i]	=	1; 
			}
		} else if(strcmp(argv[1], "OTHER") == 0) {
			printf("Completely Fair Scheduling\n");
			for(int i = 0; i < NUM_THREADS; i++) {				
				policies[i] = SCHED_OTHER;
				inputpolicy = SCHED_OTHER;
				affinities[i]	=	1;
			}
		} else {
			help();
			return FALSE;
		}
	}

	printf("Assignment_2 Threads:%d Scheduling Policy:%s\n", NUM_THREADS, POL_TO_STR(inputpolicy));

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	// For Main Thread Affinity is set to CPU 0
	if(sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {printf("%d Main Error Setting CPU Affinity\n", __LINE__);}

	trace_fd = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
	if(trace_fd < 0) {
		printf("Error Opening trace marker, try running with sudo\n");
	} else{
		printf("Successfully opened trace_marker\n");
	}

	if((set_attributes(priorities, NUM_THREADS, affinities, policies, attributes) == FALSE))
	{
		printf("Error Setting Attributes of all threads...\n");
	}

	for(int i = 0; i < NUM_THREADS; i++)
	{
		lvargs[i].thread_number = i;
		lvargs[i].thread_policy = policies[i];
		lvargs[i].thread_affinity = affinities[i];
		lvargs[i].thread_period = periods[i];
		lvargs[i].thread_priority = priorities[i];

		if(pthread_create(&thread_id[i], &attributes[i], Thread, (void*)&lvargs[i]) != 0)
		{
			printf("%d - %s %d\n", __LINE__, "Error Spawning Thread:", i);
		}
		else
		{
			trace_write("RTS_Spawned Thread_%d\n", i);
		}	
	}
	
	for(int i=0; i<NUM_THREADS; i++) {
		pthread_join(thread_id[i], NULL);
	}

	qsort(results, NUM_THREADS, sizeof(struct thread_args), comparator);
	
	for(int i=0;i<NUM_THREADS;i++)
	{
		results[i].thread_exectime = (clock_diff(results[i].thread_start_time, results[i].thread_end_time)/1.0e6);
		printf(
				"--- >>> TimeStamp:%llu Thread:%d Policy:%s Affinity:%d Priority:%d  ExecutionTime:%f Responsetime:%lldms Deadline:%lld\n",
				results[i].thread_end_timestamp, results[i].thread_number, POL_TO_STR(results[i].thread_policy), 
				results[i].thread_affinity, results[i].thread_priority, results[i].thread_exectime, 
				results[i].thread_response_time, results[i].thread_deadline
			  );
	}

	if(close(trace_fd) == 0)
	{
		printf("Successfully closed the trace file\n");
	}
	return 0;
}
