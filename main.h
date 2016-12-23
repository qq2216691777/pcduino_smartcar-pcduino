#ifndef _MAIN_H__
#define _MAIN_H__
#include "types.h"
#include "cam.h"
#include "net.h"



extern Vehicle Motor;
extern srv_main_t *srv_main;

ptr_t * epoll_add_event( int fd, uint32_t event,void (*handler)( void *arg), void *arg ,ptr_t *ptr );
void epoll_del_event(  int fd, ptr_t *ptr );
#endif

