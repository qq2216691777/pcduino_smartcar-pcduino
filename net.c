#include <sys/epoll.h>
#include "main.h"
#include "net.h"
#include "cam.h"
#include "protocol.h"

#define DEF_TCP_SRV_PORT 19868

Vehicle Motor_tmp = {0,0,0,0,0,{'\r','\n','\0'}};

extern Vehicle Motor;

void Net_rx_handler( void *arg );
void Net_tx_handler( void *arg )
{
    inet_t * inet = (inet_t *)arg;
    struct p_prot tx_port= {HEADER_P_FLAG,0,0,0};
    char* buff;
    int res;
    int my_fd;

    buff = inet->buf;
    tx_port.header = HEADER_P_FLAG;
    tx_port.status = PCDUINO_DEV;
    tx_port.len = HEADER_P_SIZE + 0;
    if(1==Motor.status)
        tx_port.status |= STM32_DEV;
    if(2==Motor.status)
    {
        tx_port.status |= STM32_DEV;
        tx_port.status |= REMOTE_DEV;
    }

    if( (srv_main->cam !=NULL)&&(srv_main->cam->camera->new_image==1) )		//cam is ok
    {
        srv_main->cam->camera->new_image = 2;   //2:send image
        //copy image to arrary


			memcpy(&buff[HEADER_P_SIZE],srv_main->cam->camera->sbuffers.start,srv_main->cam->camera->sbuffers.length);
			tx_port.len += srv_main->cam->camera->sbuffers.length;
            tx_port.status |= IMAGE_FLAG;
		//    my_fd = open("net_result.jpg", O_RDWR | O_CREAT, 0777);
    	//	write(my_fd,&buff[HEADER_P_SIZE],srv_main->cam->camera->sbuffers.length);
    	//	close(my_fd);
        srv_main->cam->camera->new_image = 0;
//		printf("send a pic  %d\r\n",srv_main->cam->camera->sbuffers.length);
	}


    tx_port.check = (int)(tx_port.header + tx_port.status + tx_port.len );
    memcpy(buff, &tx_port,HEADER_P_SIZE);
	res = send( inet->sock, buff,tx_port.len,0 );
//	printf("send %d\r\n",res);
    if( 0 > res )
    {
        perror("send");

    }

    epoll_add_event( inet->sock, EPOLLIN, &Net_rx_handler, (void *)inet,inet->ptr);

}

void Net_rx_handler( void *arg )
{
    inet_t * inet = (inet_t *)arg;
	int res;
	int nfd;
	struct u_prot *rx_prot;

	int image_fd;
	int rx_check;

	char* buff;

    nfd = inet->sock;
    buff = inet->buf;
    res = recv(nfd, buff,500,0);
    if( (res==HEADER_U_SIZE))
    {
        rx_prot = (struct u_prot *)buff;
        rx_check = (int)(rx_prot->header +rx_prot->go_back + rx_prot->left_right + rx_prot->Rotate + rx_prot->status );
        if( (rx_prot->header==HEADER_U_FLAG)&&(rx_check ==rx_prot->check) )
        {
            Motor_tmp.Forward = rx_prot->go_back;
            Motor_tmp.left = rx_prot->left_right;
            Motor_tmp.rotate = rx_prot->Rotate;

        }
    }
    else if( 0 > res )
    {
        perror("recv");

    }

	epoll_add_event( inet->sock, EPOLLOUT, &Net_tx_handler, (void *)inet,inet->ptr);
}


void Net_epoll_handler( void *arg )
{
    net_t * srv_net = (net_t *)arg;
    int len;
    struct sockaddr_in srv_addr;
    int conn_sock;


    len = sizeof(struct sockaddr);
    conn_sock = accept( srv_net->main_sock, (struct sockaddr *)(&srv_addr), &len );
    if( -1 == conn_sock )
    {
        perror("accept");
        return ;
    }
   // printf("accept\r\n");
    if( srv_net->inet.ptr!= NULL )
	{
        epoll_del_event(srv_net->inet.sock, srv_net->inet.ptr );
		epoll_del_event( srv_main->cam->camera->fd, srv_main->cam->ptr);
	}
    srv_net->inet.sock = conn_sock;
    strcpy( srv_net->inet.ip_addr,inet_ntoa(srv_addr.sin_addr) );
    printf("server ip:%s\r\n",srv_net->inet.ip_addr);
    srv_net->inet.ptr = epoll_add_event( conn_sock, EPOLLIN, &Net_rx_handler, (void *)&srv_net->inet,srv_net->inet.ptr);
	//srv_main->cam->ptr = epoll_add_event( srv_main->cam->camera->fd, EPOLLIN, &Cam_epoll_handler, (void *)srv_main->cam,srv_main->cam->ptr);
}

net_t * net_epoll_init()
{
    struct sockaddr_in my_addr;

    int res;

    net_t * srv_net = calloc(1,sizeof(net_t));


    // 1. socket 初始化
    // 1.1 创建socket
    srv_net->inet.buf = calloc(1,200000);
    srv_net->main_sock = socket(AF_INET,SOCK_STREAM,0);
    if( -1 == srv_net->main_sock )
    {
        perror("socket");

    }
    // 1.2 初始化地址
    bzero( &my_addr, sizeof(struct sockaddr_in));

    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(DEF_TCP_SRV_PORT);

    // 1.3 bind地址
    res = bind( srv_net->main_sock, (struct sockaddr *)&my_addr, sizeof(struct sockaddr));
    if( -1 == res )
    {
        perror("bind");

    }
    //监听
    res = listen( srv_net->main_sock, 5 );      //max connect number
    if( -1 == res )
    {
        perror("listen");

    }
    srv_net->ptr=epoll_add_event( srv_net->main_sock, EPOLLIN, &Net_epoll_handler, (void *)srv_net,srv_net->ptr );
    srv_net->inet.ptr = NULL;

	return srv_net;

}

