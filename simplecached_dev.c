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

shutdown_type shutdown_struct;

static void _sig_handler(int signo)
{
	if (signo == SIGINT || signo == SIGTERM){
		/* Unlink IPC mechanisms here*/
		pthread_mutex_lock(&shutdown_struct.sd_mutiex);
		shutdown_struct.flag = 1;
		pthread_cond_signal(&shutdown_struct.sd_cv);
		pthread_mutex_unlock(&shutdown_struct.sd_mutiex);
		return;
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
		if(!(strcmp("kill", (char*) client))){
//			printf("pill received, returning\n");
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
		
		printf("Ready to read data....\n");
		bytes_so_far = 0;
		while(bytes_so_far<sizeof(request_type)){
			bytes_so_far += read(cli_sock, request, sizeof(request_type));
			printf("Read so far %d bytes\n", bytes_so_far);
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



void acceptor_thread(void* acceptor_data){
	acceptor_data_type *data = acceptor_data;
	
	int fromlen;
	int sockfd, len;
	struct sockaddr_un serv_addr, client_addr;
	
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
	
	if (listen(sockfd, 5) < 0) {
	    perror("server: listen");
	    exit(1);
	  }
	
	while(1){
		printf("Accepting connetcions\n");
		
		int newsockfd;
		
	
	    /*
	    * Accept connections.  When we accept one, ns
	    * will be connected to the client.  fsaun will
	    * contain the address of the client.
	    */
	  if ((newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, &fromlen)) < 0) {
	    perror("server: accept");
	    
	  }
	  printf("Accepthed\n");
	  if (shutdown_struct.flag){
		  printf("Acceptor exiting\n");
		  close(sockfd);
		  return;
		  
	  }
	  	  
	  int * client = malloc(sizeof(int));
	   *client = newsockfd;
	  enqueue(client, data->queue);
	  printf("Enqueued\n");
	}
}

int main(int argc, char **argv) {
	
	
	int i;
	pthread_t * threadsLaunched;
	thread_queue_type worker_queue;
	
	int nthreads = 1;
	char *cachedir = "locals.txt";
	char option_char;
	pthread_t thread;
	char c;
	FILE *fp;
	int acceptor_len;
	struct sockaddr_un acceptor, kill_addr;
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
	
	//global shutdown init
	pthread_mutex_init(&shutdown_struct.sd_mutiex, NULL);
	pthread_cond_init(&shutdown_struct.sd_cv, NULL);
	shutdown_struct.flag = 0;
	
	
	acceptor.sun_family = AF_UNIX;
	strcpy(acceptor.sun_path, ADDRESS);
	
	unlink(ADDRESS);
	acceptor_len = sizeof(acceptor.sun_family) + strlen(acceptor.sun_path);
	
	// IPC socket INIT
	

	

	
	//Initializing cache
	int cache;
	cache = simplecache_init(cachedir);
	
//	printf("Cache is %d\n", cache);
	
	//Initializing pool of workers
	
	threadsLaunched = malloc(sizeof(pthread_t) * (nthreads+2));
	
	init_thread_queue(&worker_queue);
	for (i=0; i<nthreads; i++){
	    pthread_create(&thread, NULL, (void*) &test_worker_queue, (void*) &worker_queue);
		threadsLaunched[i] = thread;
	}
	threadsLaunched[nthreads+1] = NULL;
	//Initializing pool of workers END
	
	
	
	
//	 printf("Listening socket\n");
	  
	
	
	acceptor_data_type* acceptorData = malloc(sizeof(acceptor_data_type));
	acceptorData->queue = &worker_queue;
	
	
//	printf("Acceptor data: socketfd %d, client address %d, len %d\n", acceptorData->socketfd, acceptorData->client_addr, acceptorData->fromlen);
	
	pthread_create(&thread, NULL, (void*) &acceptor_thread, (void*) acceptorData);
	threadsLaunched[nthreads] = thread;
	
	
	pthread_mutex_lock(&shutdown_struct.sd_mutiex);
	while(1){
	pthread_cond_wait(&shutdown_struct.sd_cv, &shutdown_struct.sd_mutiex); 
	if (!(shutdown_struct.flag))
		continue;
	break;
	}
	pthread_mutex_unlock(&shutdown_struct.sd_mutiex);
	
//	printf("closing socket\n");
#if 0
	if(	connect(sockfd, (struct sockaddr*)&client_addr, fromlen)<0){
		perror("connect socket error");
	}
	
	if (close(acceptorData->socketfd)<0){
		perror("closing socket");
	}
#endif
	for (i=0; i<nthreads+3; i++){
		char* poison_pill = "kill";
		enqueue(poison_pill, &worker_queue);
	}
	printf("poison sent, killing acceptor\n");
	
	int kill_fd, kill_len, sock_return;
	
		
	if ((kill_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
	    perror("server: socket");
	    exit(1);
	}
	
	kill_addr.sun_family = AF_UNIX;
	strcpy(kill_addr.sun_path, "kill_socket");
	
	unlink("kill_socket");
	kill_len = sizeof(kill_addr.sun_family) + strlen(kill_addr.sun_path);

		
	
	if (sock_return = connect(kill_fd, (struct sockaddr*)&acceptor, acceptor_len)<0){
			perror("Kill acceptor, connection error");
	}
	
	
	
	close(kill_fd);
	
	
	
	while(*threadsLaunched)	  {
		pthread_join(*threadsLaunched, NULL);
		++threadsLaunched;
//		printf("thread joined\n");
	}
	printf("Joined all threads\n");
	
	simplecache_destroy();
	destroy_thread_queue(&worker_queue);
	
	printf("Looks like everybody is out. Exiting....\n");	
//	sleep(2);
	pthread_mutex_destroy(&shutdown_struct.sd_mutiex);
	pthread_cond_destroy(&shutdown_struct.sd_cv);
	free(acceptorData);
	return 0;
	
}



