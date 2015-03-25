#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


#include "shm_channel.h"
#include "simplecache.h"

#define MAX_CACHE_REQUEST_LEN 256
#define ADDRESS     "message_socket" 
#define BUFFER_SIZE 4096




static void _sig_handler(int signo)
{
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
	
	while(1){
		int *client = dequeue_blocking(queue);
		if (*client == NULL){
			free(client);
			return;
		}
		int cli_sock = *client;
		request_type* request = malloc(sizeof(request_type));
		int bytes_so_far = 0;
		int fd;
		int shmid; //memory id
		char* shm;
		char* response_codes = "fnwl";
		FILE* r_file;
		
//		printf("Ready to read data....\n");
		bytes_so_far = 0;
		while(bytes_so_far<sizeof(request_type)){
			bytes_so_far += read(cli_sock, request, sizeof(request_type));
//			printf("Read so far %d bytes\n", bytes_so_far);
		}
		printf("received request: filename: %s, shared memory key: %d, segment size %ld\n"
		, request->filename
		, (int)request->key
		, (long)request->size);
			
		//request received, looking for the file:
		fd = simplecache_get(request->filename);
		printf("Fd is %d\n", fd);
		
		// if file not found - respond, close socket
		if (fd <0){
			printf("Requested file not found\n");
			//ERROR CHECKING
			write(cli_sock, &response_codes[1], sizeof(char));
			close(cli_sock);//cleaning up
			free(client);
			free(request);
			continue;
		} else { // found
			
			
			//shm attach 
			if((shmid = shmget(request->key, request->size, 0666)) <0){
				perror("shmget");
				exit(1);				
			}
			if((shm = shmat(shmid, NULL, 0)) == (char*) -1){
				perror("shmat");
				exit(1);
			}
//			printf("Memory segment attached, ready to write data\n");
			//ERROR CHECKING
			write(cli_sock,  &response_codes[0], sizeof(char));
			
			//process writing file to memory
			r_file = fdopen(fd, "r");
			
			size_t file_len;
			file_len = lseek(fd, 0, SEEK_END);
			lseek(fd, 0, SEEK_SET);
			int bytes_to_read = (int)file_len;
			char* mBuffer = malloc(BUFFER_SIZE);
			printf("file size is %d, bytes to write is %d\n", (int) file_len, bytes_to_read);
			//init block struct:
			block_type *chunk;
			chunk =(block_type*) shm;
			int bytes_read_so_far = 0;
			int bytes_fits = request->size - sizeof(int);
			
//			printf("about to read, should fit %d\n", bytes_fits);
			
			
			//sending chunks to client
			
			while(1){
				int bytes_read_this_read = fread(&(chunk->data), 1, bytes_fits, r_file);
				bytes_read_so_far += bytes_read_this_read;
				chunk->size = bytes_read_this_read;
//				printf("read %d bytes, size %d. Informing client...\n", bytes_read_this_read, chunk->size);
				
				if (bytes_read_so_far == bytes_to_read){ //last chunk
					write(cli_sock,  &response_codes[3], sizeof(char));
					break;
				} else {
					write(cli_sock,  &response_codes[2], sizeof(char));
//					printf("Waiting for client confirmation...(\n");
					if(read(cli_sock, mBuffer, sizeof(char)) < 0){
						perror("Socket reading error");
						exit(1);
					}
//					printf("Client response is %c\n", mBuffer[0]);
					if (mBuffer[0] == 'o'){
						mBuffer[0] = '\0';
						continue;
					}					
				}				
			
			}	// file sent
			
			printf("file sent.. cleaning up...\n");
			close(cli_sock); //closing socket
			if(shmdt(shm)<0)
				perror("SHMDT failed"); // detaching segment
			free(mBuffer);
			free(request);
			free(client);
		}
		
	} //back to the loop
	return;   
}

int main(int argc, char **argv) {
	
	int nthreads = 1;
	int i;
	char *cachedir = "locals.txt";
	char option_char;
	pthread_t thread;
	char c;
	FILE *fp;
	int fromlen;
	int sockfd, newsockfd, len;
	struct sockaddr_un serv_addr, client_addr;
	char message [MAX_CACHE_REQUEST_LEN];

      
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
	
	
	
	// IPC socket INIT
	
	/*
	* Get a socket to work with.  This socket will
	* be in the UNIX domain, and will be a
	* stream socket.
	*/
	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
	    perror("server: socket");
	    exit(1);
	}
	
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, ADDRESS);
	
	unlink(ADDRESS);
	len = sizeof(serv_addr.sun_family) + strlen(serv_addr.sun_path);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, len) < 0) {
	    perror("server: bind");
	    exit(1);
	}
	
	// IPC socket INIT END
	
	
	
	printf("in main\n");
	//Initializing pool of workers
	
	thread_queue_type worker_queue;
	init_thread_queue(&worker_queue);
	for (i=0; i<nthreads; i++){
	    pthread_create(&thread, NULL, (void*) &test_worker_queue, (void*) &worker_queue);
	}
	//Initializing pool of workers END
	
	//Initializing cache
	int cache;
	cache = simplecache_init(cachedir);
	
	printf("Cache is %d\n", cache);
	
	
	
	
	
	
	
	 printf("Listening socket\n");
	  if (listen(sockfd, 5) < 0) {
	    perror("server: listen");
	    exit(1);
	  }
	
	while(1){
		printf("Accepting connetcions\n");
		int * client = malloc(sizeof(int));
	    /*
	    * Accept connections.  When we accept one, ns
	    * will be connected to the client.  fsaun will
	    * contain the address of the client.
	    */
	  if ((newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &fromlen)) < 0) {
	    perror("server: accept");
	    exit(1);
	  }
	  *client = newsockfd;
	  enqueue(client, &worker_queue);
	  
	  
	}
	
	return;
	
	
	
#if 0 
	
	
	/* Initializing the cache */
	simplecache_init(cachedir);

	//Your code here...
#endif
}



