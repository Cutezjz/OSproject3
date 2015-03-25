#ifndef SHM_CACHE_FUNCTIONS
#define SHM_CACHE_FUNCTIONS


#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>


typedef struct s_request {
   key_t key; //shm key
   long size; // segment size
   char filename [256]; }request_type;
   

  
 

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

typedef struct data {
    thread_queue_type* shm_queue; 
    long* segment_size;
  }data_type;
 
typedef struct block {
   int size;
   char data;
}block_type;

typedef struct node{
  size_t nodeSize;
  void* data;
  struct node* next;  
} node_type;

typedef struct s_in_memory_file {
  int num_nodes;
  size_t totalSize;
  node_type* head;
  node_type* tail;
} in_memory_file_type;

in_memory_file_type* data_list_init(void);
int data_list_is_empty(in_memory_file_type* list);
void data_list_add_node(in_memory_file_type* list, size_t size, void* data);
void data_list_clean(in_memory_file_type *list);
size_t data_list_write_data(void *ptr, size_t size, size_t nmemb, void *list) ;


#endif
