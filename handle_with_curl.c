#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"
#include "shm_channel.h"

//Replace with an implementation of handle_with_curl and any other
//functions you may need.


#if 0

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


#endif



ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
  
  //init//
    CURL *curl;
    CURLcode res;
    char *host = "HTTP://";
   
 //   char *request = "GET / HTTP/1.0\r\nHost: ";
    char buffer[4096];
   
    size_t bytes_transferred;
    
    ssize_t write_len;
    in_memory_file_type* dataList = data_list_init();
       
    curl = curl_easy_init();
    
   //Making url request in buffer 
    strcpy(buffer, host);
    strcat(buffer, arg);
    strcat(buffer, path);
   puts(buffer);
    
   // requesting file from url and writing it to temp file
    if (curl) {
        
        curl_easy_setopt(curl, CURLOPT_URL, buffer);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, data_list_write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, dataList);
//      curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
	res = curl_easy_perform(curl);
    }
    
    if (res != CURLE_OK){
      printf("File not found, or some problems on server\n");
      printf("Error: %s\n", curl_easy_strerror(res));
      data_list_clean(dataList);
      curl_easy_cleanup(curl);
      
      return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    }
    
// If ok, calculating length
   
    
    printf("Bytes received: %zd\n", dataList->totalSize); 
    
    gfs_sendheader(ctx, GF_OK, dataList->totalSize);
    node_type* currentNode = dataList->head;
    bytes_transferred = 0;
    
    
    while (1){
         
      void * dataBlock = currentNode->data;
      ssize_t blockSize = currentNode->nodeSize;
      write_len = gfs_send(ctx, dataBlock, blockSize);
      if (write_len != blockSize){
		    fprintf(stderr, "handle_with_file write error");
		    return EXIT_FAILURE;
	    }
      bytes_transferred += write_len;
      
      if(!(currentNode->next))
	break;
      
      currentNode = currentNode->next;
      
    }
    

    
    printf("Bytes to transfer: %zd;  Bytes transferred: %zd\n",dataList->totalSize, bytes_transferred);
    
    curl_easy_cleanup(curl);
    data_list_clean(dataList);
     
    return bytes_transferred;
	
}

#if 0
ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
    CURL *curl;
    CURLcode res;
    char *host = "https://s3.amazonaws.com/content.udacity-data.com";
    char *url = path;
    char *request = "GET / HTTP/1.0\r\nHost: ";
    char buffer[4096];
    int fildes;
    long sockextr;
    curl_socket_t sockfd;
    size_t file_len, bytes_transferred;
    size_t iolen;
    ssize_t read_len, write_len;
    curl_off_t nread;
   
    
   
    FILE *fp;
    fp = tmpfile(); 
    fildes = fileno(fp);
    curl = curl_easy_init();
    if (curl) {
        
        curl_easy_setopt(curl, CURLOPT_URL, host);
//	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
	res = curl_easy_perform(curl);
    }
    
    if(CURLE_OK != res)    {
      printf("Error: %s\n", curl_easy_strerror(res));
      return 1;
    }
    res = curl_easy_getinfo(curl, CURLINFO_LASTSOCKET, &sockextr);
 
    if(CURLE_OK != res)
    {
      printf("Error: %s\n", curl_easy_strerror(res));
      return 1;
    }
    sockfd = sockextr;
    
     if(!wait_on_socket(sockfd, 0, 60000L))    {
      printf("Error: timeout.\n");
      return 1;
    }
        
    puts("Sending request.");
    
   // strcpy(buffer, request);
    strcat(buffer, host);
    strcat(buffer, url);
  //  strcat(buffer, "\r\n\r\n");
    
    res = curl_easy_send(curl, request, strlen(request), &iolen);
 
    if(CURLE_OK != res)    {
      printf("Error: %s\n", curl_easy_strerror(res));
      return 1;
    }
    puts("Reading response.");
    
    
    if (CURLE_OK != res){
	    printf("File not found, or some problems on server\n");
	    printf("Error: %s\n", curl_easy_strerror(res));
	    curl_easy_cleanup(curl);
	    fclose(fp);
	    return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	   
    }
    rewind(fp);
    
    while(1){
      wait_on_socket(sockfd, 1, 60000L);
      res = curl_easy_recv(curl, buffer, 4096, &iolen);
      fwrite(buffer, 1, iolen, fp);
      
      if(CURLE_OK != res)
        break;
 
      nread = (curl_off_t)iolen;
      printf("Received %" CURL_FORMAT_CURL_OFF_T " bytes.\n", nread);
    }
    
    
    
    
    
	
	file_len = lseek(fildes, 0, SEEK_END);
	lseek(fildes, 0, SEEK_SET);
	
	printf("Bytes received: %zd\n", file_len); 
	
	gfs_sendheader(ctx, GF_OK, file_len);
	
	bytes_transferred = 0;
	while(bytes_transferred < file_len){
		read_len = read(fildes, buffer, 4096);
		if (read_len <= 0){
			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
			return EXIT_FAILURE;
		}
		write_len = gfs_send(ctx, buffer, read_len);
		if (write_len != read_len){
			fprintf(stderr, "handle_with_file write error");
			return EXIT_FAILURE;
		}
		bytes_transferred += write_len;
	}
	fclose(fp);
	printf("Bytes transferred: %zd\n", bytes_transferred); 
	return bytes_transferred;
}
	
	


#endif



#if 0

ssize_t handle_with_file(gfcontext_t *ctx, char *path, void* arg){
	int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	char buffer[4096];
	char *data_dir = arg;

	strcpy(buffer,data_dir);
	strcat(buffer,path);

	if( 0 > (fildes = open(buffer, O_RDONLY))){
		if (errno == ENOENT)
			/* If the file just wasn't found, then send FILE_NOT_FOUND code*/ 
			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
		else
			/* Otherwise, it must have been a server error. gfserver library will handle*/ 
			return EXIT_FAILURE;
	}

	/* Calculating the file size */
	file_len = lseek(fildes, 0, SEEK_END);
	lseek(fildes, 0, SEEK_SET);

	gfs_sendheader(ctx, GF_OK, file_len);

	/* Sending the file contents chunk by chunk. */
	bytes_transferred = 0;
	while(bytes_transferred < file_len){
		read_len = read(fildes, buffer, 4096);
		if (read_len <= 0){
			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
			return EXIT_FAILURE;
		}
		write_len = gfs_send(ctx, buffer, read_len);
		if (write_len != read_len){
			fprintf(stderr, "handle_with_file write error");
			return EXIT_FAILURE;
		}
		bytes_transferred += write_len;
	}

	return bytes_transferred;
}

#endif