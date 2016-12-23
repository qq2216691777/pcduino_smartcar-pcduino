#ifndef _CAM_H__
#define _CAM_H__
#include <stdlib.h>
#include <stdint.h>
#include "main.h"






void * Pthread_Cam( void *arg );
cam_t * cam_epoll_init();
void Cam_handler(void *arg);
void Cam_epoll_handler( void *arg );

#endif // _CAM_H__
