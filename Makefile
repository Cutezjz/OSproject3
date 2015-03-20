CFLAGS := -Wall --std=gnu99 -g -Wno-format-security
CURL_LIBS := $(shell curl-config --libs)
CURL_CFLAGS := $(shell curl-config --cflags)

ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread -lrt
endif

PROXY_OBJ := webproxy.o steque.o

all: webproxy

simplecached: simplecached_dev.c shm_channel.c shm_channel.h
	gcc simplecached_dev.c -o $@ -lpthread -g 


webproxy: $(PROXY_OBJ) handle_with_cache.o handle_with_curl.o shm_channel.o gfserver.o 
	$(CC) -o $@ $(CFLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS)

.PHONY: clean

clean:
	mv gfserver.o gfserver.tmpo 
	rm -rf *.o webproxy simplecached_dev
	mv gfserver.tmpo gfserver.o  	