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



in_memory_file_type* data_list_init(void){
  in_memory_file_type *new_list = malloc(sizeof(in_memory_file_type));
  new_list->totalSize = 0;
  new_list->head = NULL;
  new_list->tail = NULL;
  return new_list; 
}

int data_list_is_empty(in_memory_file_type* list){
  int result = (!(list->head));
  return result;
}


void data_list_add_node(in_memory_file_type* list, size_t size, void* data){
    node_type* newNode = malloc(sizeof(node_type));
    newNode->data = malloc(size);
    memcpy(newNode->data, data, size);
    newNode->nodeSize = size;
    //updating
    newNode->next = NULL;
    if (!(list->head)){
      list->head = newNode;
      list->tail = newNode;
      list->totalSize += size;
      list->num_nodes ++;
    } else{
    list->tail->next = newNode;
    list->tail = newNode;
    list->totalSize += size;
    list->num_nodes ++;
    }
    return;
}



void data_list_clean(in_memory_file_type *list){
     if(data_list_is_empty(list)){
       free(list);
       return;
    }  else {
    while(1){
	node_type* temp = list->head;
	free(temp->data);
	if (!(temp->next)){
	  free(temp);
	  break;
	}
	list->head = list->head->next;
	free(temp);
    }  
    free(list);
    return;  
    }
}


size_t data_list_write_data(void *ptr, size_t size, size_t nmemb, void *list) {
    //creating new node, adding data
    size_t dataSize = size*nmemb;
    data_list_add_node(list, dataSize, ptr);     
    //updating list
    return dataSize;
}

