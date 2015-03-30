#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

#include "gfserver.h"
#include "shm_channel.h"

#define NSTRS       3           /* no. of strings  */
#define ADDRESS      "message_socket"  /* addr to connect */
#define BUFFER_SIZE 4096

/*
 * Strings we send to the server.
 */
char *strs[NSTRS] = {
    "data"
};


//Replace with an implementation of handle_with_cache and any other
//functions you may need.


ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg){
    int shmid;//shared memory id
    //path - filename
    //arg - worker_data
    char* sh_mem_segment;
    printf("\nThread entered\n");
    char buffer [256];
    data_type* worker_data = arg;
    request_type request;
    
    key_t *key = dequeue_blocking(worker_data->shm_queue);
    
    
    request.key = *key;
    request.size =*worker_data->segment_size;
    
    
    strcpy(buffer, path);
    int i;
    for (i=0; ; i++){
      request.filename[i] = buffer[i];
      if (buffer[i] == '\0')
	break;
    }
    
	printf("request so far key is %d, size is: %ld, path is %s\n", 
	(int)request.key, request.size, request.filename);
   
    printf("Attaching to shared memory...\n");
    
    if((shmid = shmget(*key, (size_t)*worker_data->segment_size, 0666))< 0){
     perror("shmget");
     exit(1);
    }
    
    if ((sh_mem_segment = shmat(shmid, NULL, 0)) == (char *) -1){
      perror("shmat");
      exit(1);       
    }
    
    
    
    printf("Segment successfully attached. Opening socket\n");
    int socketfd, len;
    struct sockaddr_un saun;

    /*
     * Get a socket to work with.  This socket will
     * be in the UNIX domain, and will be a
     * stream socket.
     */
    if ((socketfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("client: socket");
        exit(1);
    }

    /*
     * Create the address we will be connecting to.
     */
    saun.sun_family = AF_UNIX;
    strcpy(saun.sun_path, ADDRESS);
  
    /*
     * Try to connect to the address.  For this to
     * succeed, the server must already have bound
     * this address, and must have issued a listen()
     * request.
     *
     * The third argument indicates the "length" of
     * the structure, not just the length of the
     * socket name.
     */
    
    len = sizeof(saun.sun_family) + strlen(saun.sun_path);
    
    while (connect(socketfd, (struct sockaddr*)&saun, len) < 0) {
        perror("client: connect");
	sleep(2);  
    }
	
    printf("Connected! Sending request \n");
	
    int bytes_to_write = sizeof(request);
	printf("Size of request is %d\n", bytes_to_write);
	int bytes_written = 0;
	while(bytes_written<bytes_to_write){
		if(bytes_written += write(socketfd, &request, sizeof(request))<0)
			perror("writing error");
	}
    printf("Message sent. Should have sent %d bytes. Have sent %d bytes\n", bytes_to_write, bytes_written);
	char* mBuffer = malloc(BUFFER_SIZE); //buffer to read response
	
	if(read(socketfd, mBuffer, sizeof(char)) < 0){
		perror("Socket reading error");
		exit(1);
	}
	//if not found
	if (mBuffer[0] == 'n'){ // NOT FOUND!!!!
			//notify client
			// cleanup
			printf("Not found. Cleaning up, notifying client.\n");
			close(socketfd); // close socket
			if (shmdt(sh_mem_segment)<0) // detatch memory
			perror("shmdt failed");
			free(mBuffer);
			enqueue(key, worker_data->shm_queue); //releasing shm key
			
			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	} else if(mBuffer[0] == 'f') { // FOUND!!!
		// init data list
		printf("Init datalist\n");
		in_memory_file_type* dataList;
		dataList = data_list_init();
		size_t filesize = 0;
		
		//writing data to a list
		
		while(1){
//			printf("Waiting for server to put chunk in memory...\n");		
			if(read(socketfd, mBuffer, sizeof(char)) < 0){
				perror("Socket reading error");
				exit(1);
			}
			
			if (mBuffer[0] == 'w' || mBuffer[0] == 'l'){
//				printf("Chunk is ready.. reading shm, adding to the list\n");
				// reading shm, adding to list
				block_type * chunk;
				chunk = (block_type*) sh_mem_segment;
//				printf ("The size of chunk is %d\n", chunk->size);
				filesize += data_list_write_data(&(chunk->data),1 , (size_t)chunk->size, dataList);
				
				if (mBuffer[0] == 'l'){
					mBuffer[0] = '\0';
//					printf("last chunk added\n");
					break;
				} else if (mBuffer[0] == 'w'){
					mBuffer[0] = '\0';
//					printf("chunk added, requesting more\n");
					write(socketfd, "o", sizeof(char));
				}
				
				
			}
		} // file in list
		
		printf("total received %d\n", (int)filesize);
		close(socketfd); // close socket
		if (shmdt(sh_mem_segment)<0) // detatch memory
			perror("shmdt failed");
		enqueue(key, worker_data->shm_queue); //releasing shm key
		
		
		//sending file to client
		
		gfs_sendheader(ctx, GF_OK, dataList->totalSize);
		node_type* currentNode = dataList->head;
		int bytes_transferred = 0;
		ssize_t write_len;	
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
		printf("Bytes to transfer: %zd;  Bytes transferred: %d\n",dataList->totalSize, bytes_transferred);
		data_list_clean(dataList);
		free(mBuffer);
		
		
		return bytes_transferred;
	}   
	return -1;
}
    
  



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