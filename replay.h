#ifndef _REPLAY_H
#define _REPLAY_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <pthreads.h>

#define SUCCESS 1
#define FAILURE 0
#define BUFSIZE	300

#define MEM_ALIGN				512  // Memory alignment
#define USE_GLOBAL_BUFF			1 
#define AIO_THREAD_POOL_SIZE	50

#define BYTE_PER_BLOCK			512 
#define LARGEST_REQUEST_SIZE	10000  // Largest request size in blocks
#define BLOCK_PER_DRIVE			3800000	//2GB blocks number

struct config_info{
	char device[64];
	char traceFileName[64];
	int	 deviceNum;
	char logFileName[64];
};

struct trace_info{
	double time;
	unsigned int dev;
	long long lba;
	unsigned int size;
	unsigned int type;
	struct trace_info *next;
};

struct aiocb_info{
	struct aiocb* aiocb;
	///* The order of these fields is implementation-dependent */
	//int             aio_fildes;     /* File descriptor */
	//off_t           aio_offset;     /* File offset */
	//volatile void  *aio_buf;        /* Location of buffer */
	//size_t          aio_nbytes;     /* Length of transfer */
	//int             aio_reqprio;    /* Request priority */
	//struct sigevent aio_sigevent;   /* Notification method */
	//int             aio_lio_opcode; /* Operation to be performed;lio_listio() only */
	//
	struct trace_info* req;
	int beginTime;
};

//replay.c
void replay(char *traceName,char *configName);
void config_read(struct config_info *config,const char *filename);
void trace_read(struct trace_info *front,struct trace_info *rear,const char *filename);
int time_now();
int time_elapsed(int begin);
static void IOCompleted(sigval_t sigval);
static struct aiocb_info *perform_aio(int fd, void *buf,struct trace_info *io);
static void init_aio();

//queue.c
void queue_push(struct trace_info *front,struct trace_info *rear,struct trace_info *req);
void queue_pop(struct trace_info *front,struct trace_info *rear,struct trace_info *req);
void queue_print(struct trace_info *front,struct trace_info *rear);

#endif
