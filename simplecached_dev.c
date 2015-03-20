#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>

#include "shm_channel.h"
#include "simplecache.h"

#define MAX_CACHE_REQUEST_LEN 256



static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		/* Unlink IPC mechanisms here*/
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -h                  Show this help message\n"                              

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"nthreads",           required_argument,      NULL,           't'},
  {"cachedir",           required_argument,      NULL,           'c'},
  {"help",               no_argument,            NULL,           'h'},
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}



void test_worker_queue(void* queue){
  int *num = dequeue_blocking(queue);
  printf ("Testing worker queue, printing data: %d\n", *num);
  free(num);
  return;
  
  
}

int main(int argc, char **argv) {
	int nthreads = 1;
	int i;
	char *cachedir = "locals.txt";
	char option_char;
	pthread_t thread;

      
	while ((option_char = getopt_long(argc, argv, "t:c:h", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;   
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;    
			default:
				Usage();
				exit(1);
		}
	}

	if (signal(SIGINT, _sig_handler) == SIG_ERR){
		fprintf(stderr,"Can't catch SIGINT...exiting.\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTERM, _sig_handler) == SIG_ERR){
		fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
		exit(EXIT_FAILURE);
	}
	
	printf("in main\n");
	//Initializing pool of workers
	
	thread_queue_type worker_queue;
	init_thread_queue(&worker_queue);
	
	
	
	for (i=0; i<20; i++){
	  pthread_create(&thread, NULL, (void*) &test_worker_queue, (void*) &worker_queue);
	  
	}
	
	
	
	for(i=0; i<10; i++){
	  int* num = malloc(sizeof(int));
	  *num = i;
	  enqueue(num, &worker_queue);
	  
	}
	
	while(1){}
	
	return;
	
	
	
#if 0 
	
	
	/* Initializing the cache */
	simplecache_init(cachedir);

	//Your code here...
#endif
}