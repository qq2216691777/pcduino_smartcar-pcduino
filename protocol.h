#ifndef _PROTOCOL_H__
#define _PROTOCOL_H__


#define     HEADER_U_FLAG     0X0A05050A
#define     HEADER_P_FLAG     0X050A050A

#define     HEADER_U_SIZE     28
#define     HEADER_P_SIZE     16

#define 	PCDUINO_DEV		  0X00000001
#define 	STM32_DEV		  0X00000002
#define 	REMOTE_DEV		  0X00000004
#define		IMAGE_FLAG		  0x00000008
struct u_prot{
    unsigned int header;
    int go_back;
    int left_right;
    int Rotate;
    int status;
    int check;
};

struct p_prot{
    unsigned int header;
    int status;
    int check;
    int len;
};



#endif // _PROTOCOL_H__
