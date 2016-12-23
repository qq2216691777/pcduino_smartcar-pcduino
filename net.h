#ifndef _NET_H__
#define _NET_H__

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "main.h"
#include "net.h"
net_t * net_epoll_init();
void * Pthread_Net( void *arg );
void Net_rx_handler( void *arg );
#endif // _NET_H__
