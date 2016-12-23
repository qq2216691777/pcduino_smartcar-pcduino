#ifndef _TYPES_H__
#define _TYPES_H__
#include <stdint.h>
typedef struct {
	int Forward;
	int left;
	int rotate;
	unsigned char status;
	unsigned char Check;
	char Enter[3];
}Vehicle;

typedef struct {
	int fd;
	int sec;
	int usec;
	Vehicle* Veh;
}Uart;




typedef struct {
    void (*handler)(void *arg);
    void *arg;
    uint8_t add_status;
    uint32_t event;
}ptr_t;

typedef struct {
    uint8_t* start;
    long unsigned int length;
} buffer_t;

typedef struct cam{
    int fd;
    unsigned char name[32];
    unsigned char drv[16];
    uint32_t width;
    uint32_t height;
    size_t buffer_count;
    buffer_t* buffers;
    buffer_t sbuffers;
    buffer_t head;
    unsigned char new_image;   //O:not image or image has been read  1: image can be read 2:send image
} camera_t;

typedef struct {
    camera_t *camera;
    ptr_t *ptr;
} cam_t;

typedef struct {
    int sock;
    char ip_addr[16];
    char *buf;
    ptr_t *ptr;
} inet_t;

typedef struct {
    int main_sock;
    inet_t inet;
    ptr_t *ptr;
} net_t;

typedef struct {
    int fd;
    ptr_t *ptr;
} serial_t;


typedef struct {
    int epfd;
	serial_t * serial;
    cam_t * cam;
    net_t * net;
}srv_main_t;


#endif // _TYPES_H__
