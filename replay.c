#include "replay.h"

void main()
{
	replay("22.ascii","config.ini");
}

void replay(char *traceName,char *configName)
{
	struct config_info *config;
	struct trace_info *trace;
	struct req_info *req;
	int fd;
	char *buf;
	int i,j;
	long long initTime,nowTime,reqTime,waitTime;
	
	config=(struct config_info *)malloc(sizeof(struct config_info));
	memset(config,0,sizeof(struct config_info));
	trace=(struct trace_info *)malloc(sizeof(struct trace_info));
	memset(trace,0,sizeof(struct trace_info));
	req=(struct req_info *)malloc(sizeof(struct req_info));
	memset(req,0,sizeof(struct req_info));

	config_read(config,configName);
	trace_read(trace,traceName);
//	queue_print(trace);

	fd = open(config->device, O_DIRECT | O_SYNC | O_RDWR); 
	if(fd < 0) 
	{
		fprintf(stderr, "Value of errno: %d\n", errno);
	       	printf("Cannot open\n");
       		exit(1);
	}

	if (posix_memalign((void**)&buf, MEM_ALIGN, LARGEST_REQUEST_SIZE * BYTE_PER_BLOCK))
	{
		fprintf(stderr, "Error allocating buffer\n");
		printf("allocating buffer error\n");
		return;
	}
	for(i=0;i<LARGEST_REQUEST_SIZE*BYTE_PER_BLOCK;i++)
	{
		//Generate random alphabets to write to file
		buf[i]=(char)(rand()%26+65);
		//printf("%c\n",buf[i]);
	}
	printf("222\n");

	init_aio();
	queue_print(trace);

	initTime=time_now();
	printf("initTime=%lld\n",initTime);
	while(trace->front)
	{
		nowTime=time_elapsed(initTime);
		printf("nowtime1=%lld\n",nowTime);
		queue_pop(trace,req);
		reqTime=req->time;
		printf("reqTime1=%lld\n",reqTime);
		waitTime=reqTime-nowTime;
		printf("waitTime=%lld\n",waitTime);
		//while(nowTime < reqTime)
		if(nowTime < reqTime)
		{
			usleep(waitTime);
			nowTime=time_elapsed(initTime);
		}
		printf("nowtime2=%lld\n",nowTime);
		printf("reqTime2=%lld\n",reqTime);
		printf("----------\n");
		perform_aio(fd,buf,req);
	}
	printf("begin sleepping------\n");
	sleep(5);
	free(buf);
}

void config_read(struct config_info *config,const char *filename)
{
	int name,value;
	char line[BUFSIZE];
	char *ptr;
	FILE *configFile;
	
	configFile=fopen(filename,"r");
	if(configFile==NULL)
	{
		printf("error: opening config file\n");
		exit(-1);
	}
	//read config file
	memset(line,0,sizeof(char)*BUFSIZE);
	while(fgets(line,sizeof(line),configFile))
	{
		if(line[0]=='#'||line[0]==' ') 
		{
			continue;
		}
       		ptr=strchr(line,'=');
	        if(!ptr)
		{
			continue;
		} 
        	name=ptr-line;	//the end of name string+1
       		value=name+1;	//the start of value string
	        while(line[name-1]==' ') 
		{
			name--;
		}
        	line[name]=0;

		if(strcmp(line,"device")==0)
		{
			sscanf(line+value,"%s",config->device);
			config->deviceNum++;
		}
		else if(strcmp(line,"trace")==0)
		{
			sscanf(line+value,"%s",config->traceFileName);
		}
		else if(strcmp(line,"log")==0)
		{
			sscanf(line+value,"%s",config->logFileName);
		}
		memset(line,0,sizeof(char)*BUFSIZE);
	}
	fclose(configFile);
}

void trace_read(struct trace_info *trace,const char *filename)
{
	FILE *traceFile;
	char line[BUFSIZE];
	struct req_info* req;

	traceFile=fopen(filename,"r");
	req=(struct req_info *)malloc(sizeof(struct req_info));
	if(traceFile==NULL)
	{
		printf("error: opening trace file\n");
		exit(-1);
	}
	while(fgets(line,sizeof(line),traceFile))
	{
		sscanf(line,"%lf %d %lld %d %d",&req->time,&req->dev,&req->lba,&req->size,&req->type);
		//push into request queue
		req->time=req->time*1000;	//ms-->us
		req->size=req->size*BYTE_PER_BLOCK;
		req->lba=(req->lba%BLOCK_PER_DRIVE)*BYTE_PER_BLOCK;
		queue_push(trace,req);
	}
	fclose(traceFile);
}

long long time_now()
{
	struct timeval now;
	gettimeofday(&now,NULL);
	return 1000000*now.tv_sec+now.tv_usec;	//us
}

long long time_elapsed(long long begin)
{
	return time_now()-begin;	//us
}

static void IOCompleted(sigval_t sigval)
{
	struct aiocb_info *cb;
	struct req_info *req;
	int latency;
	int error;
	int count;

	cb=(struct aiocb_info *)sigval.sival_ptr;
	latency=time_elapsed(cb->beginTime);
	printf("latency1=%d\n",latency);

	error=aio_error(cb->aiocb);
	if(error)
	{
		if(error != ECANCELED)
		{
			printf("Error completing i/o:%d\n",error);
			fprintf(stderr,"Error completing i/o:%d\n",error);
		}
		return;
	}
	count=aio_return(cb->aiocb);
	if(count<(int)cb->aiocb->aio_nbytes)
	{
		printf("Warning I/O completed:%db but requested:%ldb\n",
			count,cb->aiocb->aio_nbytes);
		fprintf(stderr, "Warning I/O completed:%db but requested:%ldb\n",
			count,cb->aiocb->aio_nbytes);
	}
	req=cb->req;
	printf("%lf,%lld,%d,%d \n",req->time,req->lba,req->size,req->type);
	printf("latency2=%d\n",latency);

	free(cb->aiocb);
	free(cb);
}

static void perform_aio(int fd, void *buf, struct req_info *req)
{
	struct aiocb_info *cb;
	char *buf_new;
	int error=0;

	cb=(struct aiocb_info *)malloc(sizeof(struct aiocb_info));
	memset(cb,0,sizeof(struct aiocb_info));//where to free this?
	cb->aiocb=(struct aiocb *)malloc(sizeof(struct aiocb));
	memset(cb->aiocb,0,sizeof(struct aiocb));//where to free this?

	cb->aiocb->aio_fildes = fd;
	cb->aiocb->aio_nbytes = req->size;
	cb->aiocb->aio_offset = req->lba;

	cb->aiocb->aio_sigevent.sigev_notify = SIGEV_THREAD;
	cb->aiocb->aio_sigevent.sigev_notify_function = IOCompleted;
	cb->aiocb->aio_sigevent.sigev_value.sival_ptr = cb;

	//write and read different buffer
	if(USE_GLOBAL_BUFF!=1)
	{
		if (posix_memalign((void**)&buf_new, MEM_ALIGN, req->size)) 
		{
			fprintf(stderr, "Error allocating buffer\n");
		}
		cb->aiocb->aio_buf = buf_new;
	}
	else
	{
		cb->aiocb->aio_buf = buf;
	}

	cb->req=req;
	cb->beginTime=time_now();

	if(req->type==1)
	{
		error=aio_write(cb->aiocb);
	}
	else if(req->type==0)
	{
		error=aio_read(cb->aiocb);
	}
	if(error)
	{
		fprintf(stderr, "Error performing i/o");
		exit(-1);
	}
}

static void init_aio()
{
	struct aioinit *aioParam;
	memset(aioParam,0,sizeof(struct aioinit));
	//two thread for each device is better
	aioParam->aio_threads = AIO_THREAD_POOL_SIZE;
	aioParam->aio_num = 2048;
	aioParam->aio_idle_time = 1;	
	aio_init(aioParam);
}

void queue_push(struct trace_info *trace,struct req_info *req)
{
	struct req_info* temp;
	temp = (struct req_info *)malloc(sizeof(struct req_info));
	temp->time = req->time;
	temp->dev = req->dev;
	temp->lba = req->lba;
	temp->size = req->size;
	temp->type = req->type;
	temp->next = NULL;
	if(trace->front == NULL && trace->rear == NULL)
	{
		trace->front = trace->rear = temp;
	}
	else
	{
		trace->rear->next = temp;
		trace->rear = temp;
	}
}

void queue_pop(struct trace_info *trace,struct req_info *req) 
{
	struct req_info* temp = trace->front;
	if(trace->front == NULL) 
	{
		printf("Queue is Empty\n");
		return;
	}
	req->time = trace->front->time;
	req->dev  = trace->front->dev;
	req->lba  = trace->front->lba;
	req->size = trace->front->size;
	req->type = trace->front->type;	
	if(trace->front == trace->rear) 
	{
		trace->front = trace->rear = NULL;
	}
	else 
	{
		trace->front = trace->front->next;
	}
	free(temp);
}


void queue_print(struct trace_info *trace)
{
	struct req_info* temp = trace->front;
	while(temp) 
	{
		printf("%lf ",temp->time);
		printf("%d ",temp->dev);
		printf("%lld ",temp->lba);
		printf("%d ",temp->size);
		printf("%d\n",temp->type);
		temp = temp->next;
	}
}
