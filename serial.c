#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>

#include "serial.h"
#include "main.h" 

Uart serial_usb={ -1,0,0,NULL};

const char *Serial_Dev = "/dev/ttyUSB0";

int set_opt(int fd,int nSpeed,int nBits,char nEvent,int nStop)
{
    struct termios newtio,oldtio;
    if(tcgetattr(fd,&oldtio)!=0)
    {
        perror("error:SetupSerial 3\n");
        return -1;
    }
    bzero(&newtio,sizeof(newtio));
    //使能串口接收
    newtio.c_cflag |= CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;

    newtio.c_lflag &=~ICANON;//原始模式

    //newtio.c_lflag |=ICANON; //标准模式

    //设置串口数据位
    switch(nBits)
    {
        case 7:
            newtio.c_cflag |= CS7;
            break;
        case 8:
            newtio.c_cflag |=CS8;
            break;
    }
    //设置奇偶校验位
    switch(nEvent)

    {
        case 'O':
            newtio.c_cflag |= PARENB;
            newtio.c_cflag |= PARODD;
            newtio.c_iflag |= (INPCK | ISTRIP);
            break;
        case 'E':
            newtio.c_iflag |= (INPCK | ISTRIP);
            newtio.c_cflag |= PARENB;
            newtio.c_cflag &= ~PARODD;
            break;
        case 'N':
            newtio.c_cflag &=~PARENB;
			newtio.c_iflag |= INPCK;
			newtio.c_iflag &= ~(INLCR|ICRNL|IXON|ECHO|ISIG);
			newtio.c_oflag &= ~OPOST;
            break;
    }
    //设置串口波特率
    switch(nSpeed)
    {
        case 2400:
            cfsetispeed(&newtio,B2400);
            cfsetospeed(&newtio,B2400);
            break;
        case 4800:
            cfsetispeed(&newtio,B4800);
            cfsetospeed(&newtio,B4800);
            break;
        case 9600:
            cfsetispeed(&newtio,B9600);
            cfsetospeed(&newtio,B9600);
            break;
        case 115200:
            cfsetispeed(&newtio,B115200);
            cfsetospeed(&newtio,B115200);
            break;
        case 460800:
            cfsetispeed(&newtio,B460800);
            cfsetospeed(&newtio,B460800);
            break;
        default:
            cfsetispeed(&newtio,B9600);
            cfsetospeed(&newtio,B9600);
            break;
    }
    //设置停止位
    if(nStop == 1)
        newtio.c_cflag &= ~CSTOPB;
    else if(nStop == 2)
        newtio.c_cflag |= CSTOPB;
    newtio.c_cc[VTIME] = 1;
    newtio.c_cc[VMIN] = 0;
    tcflush(fd,TCIFLUSH);

    if(tcsetattr(fd,TCSANOW,&newtio)!=0)
    {
        perror("com set error\n");
        return -1;
    }
    return 0;
}
extern Vehicle Motor_tmp;
void * Pthread_Serial_Rx( void *arg  )
{
	int serfd;
	int retval;
	struct timeval tv;
	fd_set rfds;
	Uart  *ser = (Uart *)arg;
	char Rx_data[100];	
	int len;
	int Num=0;
	
	serfd = ser->fd;
	tv.tv_sec = ser->sec;
	tv.tv_usec = ser->usec;
	while(1)
	{
		FD_ZERO(&rfds);
		FD_SET( serfd,&rfds );
		retval = select(serfd+1,&rfds,NULL,NULL,&tv);
		if( retval == -1 )
		{
			printf("error\r\n");
			break;
		}
		else if( retval)
		{
			len = read(serfd,Rx_data,20);
			//printf("rx %d\r\n",len);
			if( (len ==3 )&&( Rx_data[0] == 'S' ) )
			{
				if( Rx_data[1] == '1' )
					ser->Veh->status = 1;
				else 
					ser->Veh->status = 2;		
				Num=0;
			}
			else
			{
				usleep(1000);
				Num++;
			}	
		}
		else
		{
			usleep(1000);
			Num++;
		}	

		if( Num>100)	
		{
			ser->Veh->status = 0;
			Num=120;
		}	
	}
	pthread_exit(NULL);

}

void * Pthread_Serial( void *arg )
{
	int n=0;
	int fd;
	pthread_t pthread_id;
	
	fd = open( Serial_Dev, O_RDWR|O_NOCTTY|O_NDELAY );
    if( -1==fd ) 
    	pthread_exit(NULL);
	
	if( set_opt(fd,115200,8,'N',1)== -1)
    {
         pthread_exit(NULL);
    }
	serial_usb.fd = fd;
	serial_usb.sec = 0;
	serial_usb.usec = 1;
	serial_usb.Veh = &Motor;
	
    pthread_create( &pthread_id, NULL, &Pthread_Serial_Rx, ( void *)&serial_usb );

	while( 0==pthread_kill(pthread_id,0)  )
	{		
		if(Motor.status)
		{
			Motor.Forward = Motor_tmp.Forward;
			Motor.left = Motor_tmp.left;
			Motor.rotate = Motor_tmp.rotate;
			Motor.Check = (unsigned char)(Motor.Forward + Motor.left +	Motor.rotate);
			write( fd, &Motor, 16 );
			
				//serial_send( fd, "this is ok\r\n" );
			//printf("write serial\r\n");
		}
	//	printf("tx send %d\r\n",Motor.status);
		usleep(50000);
	}

	printf("receive thread is quited\r\n");
	pthread_exit(NULL);
}
void Serial_Rx_handler( void *arg )
{
	serial_t *serial = (serial_t *)arg;
	char Rx_data[100];
	int len;
	Uart  *ser = &serial_usb;
	
	len = read(serial->fd,Rx_data,20);
	if( len<0)	
		perror("rx");
	printf("rx %d\r\n",len);
	if( (len ==3 )&&( Rx_data[0] == 'S' ) )
	{
		if( Rx_data[1] == '1' )
			ser->Veh->status = 1;
		else 
			ser->Veh->status = 2;		
	}


}


serial_t * serial_epoll_init()
{
	serial_t *serial = calloc(1,sizeof(serial_t));
	int n=0;
	
	pthread_t pthread_id;
	
	serial->fd = open( Serial_Dev, O_RDWR );
    if( -1==serial->fd ) 
    	perror("serial open");
	
	if( set_opt(serial->fd,115200,8,'N',1)== -1)
    {
         perror("set opt");
    }
	serial_usb.fd = serial->fd;
	serial_usb.sec = 0;
	serial_usb.usec = 1;
	serial_usb.Veh = &Motor;
	
	serial->ptr = epoll_add_event( serial->fd, EPOLLIN, &Serial_Rx_handler, (void *)serial,serial->ptr);

	return serial;
	

}

