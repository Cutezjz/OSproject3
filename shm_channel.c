//In case you want to implement the shared memory IPC as a library...
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <error.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "shm_channel.h"






//struct thread_queue_node_type;




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


int q_is_empty(thread_queue_type* queue) {
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
