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

typedef struct thread_queue_node {
	void* data;
	struct thread_queue_node* next;
	
} thread_queue_node_type;


typedef struct thread_queue {
	pthread_mutex_t mutex;
	pthread_cond_t c;
	thread_queue_node_type* first_node;
	thread_queue_node_type* last_node;
} thread_queue_type;

void init_thread_queue(thread_queue_type* queue);


//empty
int queue_worker_is_empty(thread_queue_type* queue);
int is_empty(thread_queue_type* queue);

//enqueue
void enqueue(void* data, thread_queue_type* queue);


//dequeue
void* deq_worker(thread_queue_type* queue);
void* dequeue_blocking(thread_queue_type* queue);
void* dequeue_non_blocking(thread_queue_type* queue);





void init_thread_queue(thread_queue_type* queue) {
	queue->first_node = NULL;
	queue->last_node = NULL;
	pthread_mutex_init(&queue->mutex, NULL);
	pthread_cond_init(&queue->c, NULL);
}

int queue_worker_is_empty(thread_queue_type* queue){
	int empty = 0;
	empty = queue->first_node == NULL;
	return empty;
}


int is_empty(thread_queue_type* queue) {
	int empty = 0;
	pthread_mutex_lock(&queue->mutex);
	empty = queue_worker_is_empty(queue);
	pthread_mutex_unlock(&queue->mutex);
	return empty;
}

void enqueue(void* data, thread_queue_type* queue) {
	thread_queue_node_type* new_element = malloc(sizeof(thread_queue_node_type));
	//printf("entering enqueue\n");
	pthread_mutex_lock(&queue->mutex);
	new_element->data = data;
	if (queue->first_node == NULL) {
		new_element->next = NULL;
		queue->first_node = queue->last_node = new_element;
	} else {
		queue->last_node->next = new_element;
		new_element->next = NULL;
		queue->last_node = new_element;
	}
	pthread_cond_broadcast(&queue->c);
	pthread_mutex_unlock(&queue->mutex);
	//printf("exiting enqueue\n");
}

void* deq_worker(thread_queue_type* queue) {
	  void* result = NULL;
	  thread_queue_node_type *next = NULL;
	  if (queue->first_node != NULL) {
		  result = queue->first_node->data;
		  next = queue->first_node->next;
		  free(queue->first_node);
		  if ((queue->first_node = next) == NULL) {
		    queue->last_node = NULL;
			} else if(queue->first_node->next == NULL) {
				queue->last_node = queue->first_node;
			}
		}
	
	return result;
}

void* dequeue_blocking(thread_queue_type* queue) {
	void* result = NULL;
	thread_queue_node_type *next = NULL;
	pthread_mutex_lock(&queue->mutex);
	while(1){
		if (queue_worker_is_empty(queue)){
			pthread_cond_wait(&queue->c, &queue->mutex);
		} else {
			break;
		}
		
	}
	result = deq_worker(queue);
	pthread_mutex_unlock(&queue->mutex);
	return result;
	
}

void* dequeue_non_blocking(thread_queue_type* queue) {
		void* result = NULL;
		thread_queue_node_type *next = NULL;
		pthread_mutex_lock(&queue->mutex);
		result = deq_worker(queue);
		
		pthread_mutex_unlock(&queue->mutex);
		return result;
	
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