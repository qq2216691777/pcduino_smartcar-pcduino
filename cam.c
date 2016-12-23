/*
 * capturing from UVC cam
 * requires: libjpeg-dev
 * build: gcc -std=c99 capture.c -ljpeg -o capture
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <linux/videodev2.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/epoll.h>

#include <jpeglib.h>
#include "main.h"
#include "cam.h"

extern srv_main_t *srv_main;


camera_t* glo_camera;

static int xioctl(int fd, int request, void* arg)
{

    int r = ioctl(fd, request, arg);
    if (r != -1 || errno != EINTR)
        return r;

    return -1;
}

static camera_t* camera_open(const char * device, uint32_t width, uint32_t height)
{
    int fd = open(device, O_RDWR | O_NONBLOCK, 0);
    if (fd == -1) perror("error: open camera failed");
    camera_t* camera = malloc(sizeof (camera_t));
    camera->fd = fd;
    camera->width = width;
    camera->height = height;
    camera->buffer_count = 0;
    camera->buffers = NULL;
    camera->head.length = 0;
    camera->head.start = NULL;

    return camera;
}


static void camera_init(camera_t* camera)
{
    struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format format;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;

	size_t buf_max = 0;
    size_t i;

    if (xioctl(camera->fd, VIDIOC_QUERYCAP, &cap) == -1) perror("VIDIOC_QUERYCAP");
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) perror("no capture");
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) perror("no streaming");

    strcpy((char *)camera->name,(char *)cap.card);
    strcpy((char *)camera->drv,(char *)cap.driver);

    printf("info: camera name:%s\r\n",camera->name);
    printf("info: camera driver:%s\r\n",camera->drv);
    if( (NULL==strstr((camera->drv),"UVC")) && (NULL==strstr((camera->drv), "uvc")))
    {
	   printf("error: this is not a uvc camera");
	   return ;
	}
    memset(&cropcap, 0, sizeof cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(camera->fd, VIDIOC_CROPCAP, &cropcap) == 0)
    {

        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;
        if (xioctl(camera->fd, VIDIOC_S_CROP, &crop) == -1)
        {
          // cropping not supported
        }
    }
    memset(&format, 0, sizeof format);
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = camera->width;
    format.fmt.pix.height = camera->height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(camera->fd, VIDIOC_S_FMT, &format) == -1) perror("VIDIOC_S_FMT");


    memset(&req, 0, sizeof req);
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(camera->fd, VIDIOC_REQBUFS, &req) == -1) perror("VIDIOC_REQBUFS");
    camera->buffer_count = req.count;
    camera->buffers = calloc(req.count, sizeof (buffer_t));

    for (i = 0; i < camera->buffer_count; i++)
    {
        memset(&buf, 0, sizeof buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(camera->fd, VIDIOC_QUERYBUF, &buf) == -1)
          perror("VIDIOC_QUERYBUF");
        if (buf.length > buf_max) buf_max = buf.length;
        camera->buffers[i].length = buf.length;
        camera->buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
               camera->fd, buf.m.offset);
        if (camera->buffers[i].start == MAP_FAILED)
            perror("mmap");
    }
    camera->head.start = malloc(buf_max);
}


static void camera_start(camera_t* camera)
{
    size_t i;
    struct v4l2_buffer buf;
    for ( i = 0; i < camera->buffer_count; i++)
    {

        memset(&buf, 0, sizeof buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1)
            perror("VIDIOC_QBUF");
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(camera->fd, VIDIOC_STREAMON, &type) == -1)
    	perror("VIDIOC_STREAMON");
}

static void camera_stop(camera_t* camera)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(camera->fd, VIDIOC_STREAMOFF, &type) == -1)
        perror("VIDIOC_STREAMOFF");
}

static void camera_finish(camera_t* camera)
{
	size_t i;
    for ( i = 0; i < camera->buffer_count; i++)
    {
        munmap(camera->buffers[i].start, camera->buffers[i].length);
    }
    free(camera->buffers);
    camera->buffer_count = 0;
    camera->buffers = NULL;
    free(camera->head.start);
    camera->head.length = 0;
    camera->head.start = NULL;
}

static void camera_close(camera_t* camera)
{
  if (close(camera->fd) == -1)
    perror("close");
  free(camera);
}


static int camera_capture(camera_t* camera)
{
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof buf);
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  if (xioctl(camera->fd, VIDIOC_DQBUF, &buf) == -1)
    perror("capture ioctl");
  memcpy(camera->head.start, camera->buffers[buf.index].start, buf.bytesused);
  camera->head.length = buf.bytesused;
  if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1)
    perror("capture QBUF");

  return TRUE;
}


static void jpeg( uint8_t* rgb, uint32_t width, uint32_t height, int quality,camera_t* camera)
{
  JSAMPARRAY image;
  long  sizeddd;
  size_t i,j;
  struct jpeg_compress_struct compress;
  struct jpeg_error_mgr error;

  image = calloc(height, sizeof (JSAMPROW));
  for (i = 0; i < height; i++) {
    image[i] = calloc(width * 3, sizeof (JSAMPLE));
    for ( j = 0; j < width; j++) {
      image[i][j * 3 + 0] = rgb[(i * width + j) * 3 + 0];
      image[i][j * 3 + 1] = rgb[(i * width + j) * 3 + 1];
      image[i][j * 3 + 2] = rgb[(i * width + j) * 3 + 2];
    }
  }

//第一步：分配并初始化一个JPEG的压缩对象

  compress.err = jpeg_std_error(&error);
  jpeg_create_compress(&compress);

  //第二步：指定转换后数据的目的地
//  jpeg_stdio_dest(&compress, dest);
  jpeg_mem_dest(&compress,&camera->sbuffers.start,&camera->sbuffers.length);
  //第三步：设置压缩参数
  compress.image_width = width;
  compress.image_height = height;
  compress.input_components = 3;
  compress.in_color_space = JCS_RGB;
  jpeg_set_defaults(&compress);
  jpeg_set_quality(&compress, quality, TRUE);

  //第四步：开始压缩
  jpeg_start_compress(&compress, TRUE);

  //第五步：这里的循环参数是库的一个状态变量cinfo.next_scanline
  jpeg_write_scanlines(&compress, image, height);

  //第六步：完成压缩
  jpeg_finish_compress(&compress);
  jpeg_destroy_compress(&compress);

  for (i = 0; i < height; i++) {
    free(image[i]);
  }
  free(image);
}


static int minmax(int min, int v, int max)
{
  return (v < min) ? min : (max < v) ? max : v;
}

static uint8_t* yuyv2rgb(uint8_t* yuyv, uint32_t width, uint32_t height)
{
  uint8_t* rgb = calloc(width * height * 3, sizeof (uint8_t));
  size_t i ,j;
  size_t index;
  int y0;
  int u ;
  int y1 ;
  int v ;

  for (i = 0; i < height; i++) {
    for ( j = 0; j < width; j += 2) {
      index = i * width + j;
      y0 = yuyv[index * 2 + 0] << 8;
      u = yuyv[index * 2 + 1] - 128;
      y1 = yuyv[index * 2 + 2] << 8;
      v = yuyv[index * 2 + 3] - 128;
      rgb[index * 3 + 0] = minmax(0, (y0 + 359 * v) >> 8, 255);
      rgb[index * 3 + 1] = minmax(0, (y0 + 88 * v - 183 * u) >> 8, 255);
      rgb[index * 3 + 2] = minmax(0, (y0 + 454 * u) >> 8, 255);
      rgb[index * 3 + 3] = minmax(0, (y1 + 359 * v) >> 8, 255);
      rgb[index * 3 + 4] = minmax(0, (y1 + 88 * v - 183 * u) >> 8, 255);
      rgb[index * 3 + 5] = minmax(0, (y1 + 454 * u) >> 8, 255);
    }
  }
  return rgb;
}
#define  IMAGE_WITH        320
#define  IMAGE_HEIGH       240
struct cam * cam_sys_init()
{
    camera_t* camera = camera_open("/dev/video0", IMAGE_WITH, IMAGE_HEIGH);
    camera_init(camera);
    camera_start(camera);

    camera->sbuffers.start = calloc(1,IMAGE_HEIGH*IMAGE_WITH/2);


    return camera;
}


void v4l2_start_capture( int fd )
{
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl ( fd, VIDIOC_STREAMON, &type);
}
char read_image = 0;
void Cam_epoll_handler( void *arg )
{
	cam_t *epoll_cam = (cam_t *)arg;
	if( read_image )
		return ;
	if( FALSE == camera_capture(epoll_cam->camera) )
		return ;
	read_image = 1;
}

void * Pthread_Cam( void *arg )
{
    while( 1 )
    {
        if( read_image )
        {
            
            Cam_handler(arg);
			read_image = 0;
        }
        usleep(50000);
    }
}

cam_t * cam_epoll_init()
{
    cam_t *epoll_cam = calloc(1,sizeof(cam_t));
    pthread_t pthread_Cam_id;

	epoll_cam->camera = cam_sys_init();
    glo_camera = epoll_cam->camera;

 //   epoll_cam->ptr = epoll_add_event( epoll_cam->camera->fd, EPOLLIN, &Cam_epoll_handler, (void *)epoll_cam,epoll_cam->ptr);
    pthread_create( &pthread_Cam_id, NULL, &Pthread_Cam, epoll_cam );
    return epoll_cam;
}
//Cam_epoll_init
void Cam_handler( void *arg )
{
    cam_t *epoll_cam = (cam_t *)arg;
   unsigned char* rgb;
   static int num=0;
	int my_fd;
    camera_t* camera;
    camera = epoll_cam->camera;

    
    rgb = yuyv2rgb(camera->head.start, camera->width, camera->height);
    while( epoll_cam->camera->new_image==2 )        //等待image to be  read
        usleep(1000);
	epoll_cam->camera->new_image=0; 
    jpeg( rgb, camera->width, camera->height, 100, camera);
    free(rgb);
    epoll_cam->camera->new_image = 1;
	
    /*my_fd = open("result.jpg", O_RDWR | O_CREAT, 0777);
    write(my_fd,camera->sbuffers.start,camera->sbuffers.length);
    close(my_fd);*/
//	printf("get a picture %d\r\n",num++);

}




