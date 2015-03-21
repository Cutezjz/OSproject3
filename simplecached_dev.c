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
	char c;
	FILE *fp;
	int fromlen;
	int mess_socket, ns, len;
	struct sockaddr_un saun, fsaun;
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
	if ((mess_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
	    perror("server: socket");
	    exit(1);
	}
	
	saun.sun_family = AF_UNIX;
	strcpy(saun.sun_path, ADDRESS);
	
	unlink(ADDRESS);
	len = sizeof(saun.sun_family) + strlen(saun.sun_path);

	if (bind(mess_socket, &saun, len) < 0) {
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
	
	
	
	int request_id = 1;
	while(1){
	  printf("Listening socket\n");
	  if (listen(mess_socket, 5) < 0) {
	    perror("server: listen");
	    exit(1);
	  }

	    /*
	    * Accept connections.  When we accept one, ns
	    * will be connected to the client.  fsaun will
	    * contain the address of the client.
	    */
	  if ((ns = accept(mess_socket, &fsaun, &fromlen)) < 0) {
	    perror("server: accept");
	    exit(1);
	  }
	    
	  fp = fdopen(ns, "r");
	    
	  while ((c = fgetc(fp)) != EOF) {
		putchar(c);

		if (c == '\n')
		
		    break;
	  }
	  printf("\nRequest %d completed!\n\n", request_id);
	  request_id ++;
	  
	}
	
	return;
	
	
	
#if 0 
	
	
	/* Initializing the cache */
	simplecache_init(cachedir);

	//Your code here...
#endif
}