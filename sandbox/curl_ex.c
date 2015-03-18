#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>



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


in_memory_file_type* data_list_init(void){
  in_memory_file_type *new_list = malloc(sizeof(in_memory_file_type));
  new_list->totalSize = 0;
  new_list->head = NULL;
  new_list->tail = NULL;
  return new_list; 
}

int is_empty(in_memory_file_type* list){
  int result = (!(list->head));
  return result;
}


void add_node(in_memory_file_type* list, size_t size, void* data){
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
     if(is_empty(list)){
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


size_t write_data(void *ptr, size_t size, size_t nmemb, void *list) {
    //creating new node, adding data
    size_t dataSize = size*nmemb;
    add_node(list, dataSize, ptr);     
    //updating list
    return dataSize;
}




int main(void) {
    
    in_memory_file_type* dataList = data_list_init();
   
    CURL *curl;
    CURLcode res;
    char *url = "https://s3.amazonaws.com/content.udacity-data.com/courses/ud923/filecorpus/1kb-sa55mple-file-1.html";
    FILE* fp;
    fp = fopen("output", "wb");
     
    curl = curl_easy_init();
    if (curl) {
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, dataList);
 //       res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
        
    long dataSize = dataList->totalSize;
    
    
    printf("Bytes in list: %zd, nodes: %d\n", dataList->totalSize, dataList->num_nodes);
    node_type* pos = dataList->head;
    while(1){
      printf("Writing data\n");
      if (is_empty(dataList))
	break;
      fwrite (pos->data, 1, pos->nodeSize, fp);
      printf("Wrote data\n");
      if(!(pos->next))
	break;
      pos = pos->next;
    }
    
    
    fclose(fp);
    printf("About to clean list\n");
    data_list_clean(dataList);
    return 0;
}



#if 0

int main(void){
  printf("Inint list\n");
  in_memory_file_type* dataList = data_list_init();
  
  char *chunk1 = "I am first chunk of data. ";
  char *chunk2 = "I am second chunk of data. ";
  char *chunk3 = "I am third chunk of data. ";
  printf("Adding nodes\n");
  add_node(dataList, strlen(chunk1), chunk1);
  add_node(dataList, strlen(chunk2), chunk2);
  add_node(dataList, strlen(chunk3), chunk3);
  
  
  printf("Printing from list\n");
  node_type* pos = dataList->head;
  
  while(1){
    puts(pos->data);
    if (!(pos->next)){
      break;      
    }
    pos = pos->next;
  }
  
  
  printf("Cleaning list\n");
  data_list_clean(dataList);
  return 0;
  
}


#endif





#if 0

int main(void) {
    CURL *curl;
    FILE *fp;
    CURLcode res;
    char *url = "http://localhost/aaa.txt";
    char outfilename[FILENAME_MAX] = "C:\\bbb.txt";
    curl = curl_easy_init();
    if (curl) {
        fp = tmpfile();
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
    }
    return 0;
}

#endif


    
 