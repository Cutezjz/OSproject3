#ifndef SHM_CACHE_FUNCTIONS
#define SHM_CACHE_FUNCTIONS


#include <pthread.h>


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
int q_is_empty(thread_queue_type* queue);

//enqueue
void enqueue(void* data, thread_queue_type* queue);


//dequeue
void* deq_worker(thread_queue_type* queue);
void* dequeue_blocking(thread_queue_type* queue);
void* dequeue_non_blocking(thread_queue_type* queue);

#endif
