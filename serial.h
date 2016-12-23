#ifndef _SERIAL_H__
#define _SERIAL_H__
#include "types.h"


int set_opt(int fd,int nSpeed,int nBits,char nEvent,int nStop);
void * Pthread_Serial( void *arg );
serial_t * serial_epoll_init();
#endif

