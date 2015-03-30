#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <curl/curl.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "gfserver.h"
#include "shm_channel.h"
                                                                \
#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webproxy [options]\n"                                                     \
"options:\n"                                                                  \
"  -p [listen_port]    Listen port (Default: 8888)\n"                         \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)"\
"  -n [num_segments]   The number of shared memory segments"                  \
"  -z [segment_size]   Size of each segment"                                  \
"  -h                  Show this help message\n"                              \
"special options:\n"                                                          \
"  -d [drop_factor]    Drop connects if f*t pending requests (Default: 5).\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"server",        required_argument,      NULL,           's'},         
  {"num_segments",  required_argument, 	    NULL,           'n'},
  {"segment_size",  required_argument,      NULL,           'z'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};

extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);

static gfserver_t gfs;
static thread_queue_type shm_queue;
long *size;


static void _sig_handler(int signo){
  if (signo == SIGINT || signo == SIGTERM){
    
	while(!(q_is_empty(&shm_queue))){
		int shmid;
		key_t* destroy = dequeue_blocking(&shm_queue);
		shmid = shmget(*destroy, *size, 0666);
		shmctl(shmid, IPC_RMID, 0);
		free(destroy);
//		printf("segment free\n");
	}
	gfserver_stop(&gfs);
	printf("Segments destroyed, server stopped, exiting...\n");
	exit(signo);
  }
}




/* Main ========================================================= */
int main(int argc, char **argv) {
  data_type* worker_data = malloc(sizeof(data_type));
  int i, option_char = 0;
  unsigned short port = 8888;
  unsigned short nworkerthreads = 1;
  int num_segments = 10;
  long segment_size = 8192;
  size = &segment_size;
  int shmid;
  
  init_thread_queue(&shm_queue);
  
  char* server = "http://s3.amazonaws.com/content.udacity-data.com";
  printf ("Server is %s\n", server);
  
  
  

  if (signal(SIGINT, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:t:n:z:s:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 'n': // number of segments
			num_segments = atoi(optarg);
			break;
      case 'z': // segment_size
			segment_size = atoi(optarg);
			break;
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 's': // file-path
        server = optarg;
        break;                                          
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;       
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
    }
  }
  
  /* SHM initialization...*/
  
  
  for (i=0; i<num_segments; i++){
    key_t* key = malloc(sizeof(key_t));
    *key = 5000+i;
//  printf("Creating segment, key is %d\n", *key);  
    if ((shmid = shmget(*key, segment_size, IPC_CREAT | 0666)) < 0) {
      perror("shmget");
      exit(1);
    }
    enqueue(key, &shm_queue);
   
        
  }
  
  worker_data->shm_queue = &shm_queue;
  worker_data->segment_size = &segment_size;
  
  
 
  

  /*Initializing server*/
  gfserver_init(&gfs, nworkerthreads);
//curl_global_init(CURL_GLOBAL_ALL);
  /*Setting options*/
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 10);
  
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
  
  for(i = 0; i < nworkerthreads; i++)
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, worker_data);

  /*Loops forever*/
  gfserver_serve(&gfs);
}