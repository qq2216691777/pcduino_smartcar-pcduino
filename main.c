#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include "serial.h"

#include "main.h"
#include "net.h"
#include "cam.h"

srv_main_t *srv_main;
Vehicle Motor = {0,0,0,0,0,{'\r','\n','\0'}};

ptr_t * epoll_add_event( int fd, uint32_t event,void (*handler)( void *arg), void *arg ,ptr_t *ptr )
{
    struct epoll_event epv;
    int op;

    if( ptr == NULL )
    {
        ptr = calloc(1,sizeof(ptr_t));
        op = EPOLL_CTL_ADD;
    }
    else
        op = EPOLL_CTL_MOD;
    if( ptr->add_status == 1)
    {
		op = EPOLL_CTL_MOD;
    }else{
    	op = EPOLL_CTL_ADD;
    }
    epv.events = event;
    epv.data.ptr = ptr;
    ptr->arg = arg;
    ptr->handler = handler;
    ptr->event = event;
    ptr->add_status = 1;

    if(-1 == epoll_ctl( srv_main->epfd, op, fd, &epv ) )
		perror("add epoll_ctl");

	return ptr;
}

void epoll_del_event(  int fd, ptr_t *ptr )
{
	struct epoll_event epv;
	int op;

    op = EPOLL_CTL_DEL;
	epv.events = ptr->event;
	if(-1 == epoll_ctl(srv_main->epfd,op,fd,&epv))
		perror("del epoll_ctl");
	else
		printf("del successed\r\n");

    ptr->add_status = 0;

}


extern Vehicle Motor_tmp;
int main()
{
    pthread_t pthread_Serial_id;
    struct epoll_event events[512];
    ptr_t *ptr;
    int fds;
    uint32_t event;
    int i;
	int res;
	struct timeval times;
	struct timeval timed;

    srv_main = calloc(1,sizeof(srv_main_t));

    srv_main->epfd = epoll_create(512);

	pthread_create( &pthread_Serial_id, NULL, &Pthread_Serial, NULL );
	usleep(10000);

	if( 0!= pthread_kill(pthread_Serial_id,0))
	{
		printf("error: cannot open serial dev\r\n");
		return -1;
	}
//	srv_main->serial = serial_epoll_init();

//    srv_main->cam = cam_epoll_init();

    srv_main->net = net_epoll_init();

	gettimeofday(&times,0);
    printf("info: all device init is ok\r\n");
    while(1)
    {

        fds = epoll_wait(srv_main->epfd,events,512,1000);

        for(i=0;i<fds;i++)
     	{
             event = events[i].events;
             ptr = events[i].data.ptr;
             if( (event&EPOLLIN)&&(ptr->event&EPOLLIN) )
                 ptr->handler( ptr->arg );

             if( (event&EPOLLOUT)&&(ptr->event&EPOLLOUT) )
                 ptr->handler( ptr->arg );

             if( (event&EPOLLERR)&&(ptr->event&EPOLLERR) )
                ptr->handler( ptr->arg );
     	}
		if( Motor.status )
		{
			gettimeofday(&timed,0);
			if( (timed.tv_usec - times.tv_usec) >100000 )
			{
				Motor.status =0;
				gettimeofday(&times,0);
			}
			else
			{
				Motor.Forward = Motor_tmp.Forward;
				Motor.left = Motor_tmp.left;
				Motor.rotate = Motor_tmp.rotate;
				Motor.Check = (unsigned char)(Motor.Forward + Motor.left +	Motor.rotate);
				res = write( srv_main->serial->fd, &Motor, 16 );
				if( res < 0 )
					perror("serial write");
				printf("tx %d\r\n",res);
			}
		}
		else
			gettimeofday(&times,0);
    }

    return 0;
}
