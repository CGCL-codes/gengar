#include <stdio.h>

#include "dhmp.h"

const int obj_num=15000;

int main(int argc,char *argv[])
{
	void *addr[obj_num];
	int i,size=1048576,rnum,readnum,writenum;
	char *str;
	struct timespec task_time_start, task_time_end;
	unsigned long task_time_diff_ns;
	
	if(argc<3)
	{
		printf("input param error. input:<filname> <readnum> <writenum> \n");
		return -1;
	}
	else
	{
		readnum=atoi(argv[1]);
		writenum=atoi(argv[2]);
	}

	str=malloc(size);
	if(!str){
		printf("alloc mem error");
		return -1;
	}
	snprintf(str, size, "hello world hello world hello world hello world hello world");
	
	dhmp_client_init();
	
	for(i=0;i<obj_num;i++)
	{
		if(i%1000==0)
			sleep(1);
		addr[i]=dhmp_malloc(size);
	}
	clock_gettime(CLOCK_MONOTONIC, &task_time_start);
	
	for(i=0;i<readnum;i++)
	{
		rnum=rand()%obj_num;
		dhmp_read(addr[rnum], str, size);
	}
	
	for(i=0;i<writenum;i++)
	{
		rnum=rand()%obj_num;
		dhmp_write(addr[rnum], str, size);
	}
	
	clock_gettime(CLOCK_MONOTONIC, &task_time_end);

	sleep(3);
	for(i=0;i<obj_num;i++)
	{
		if(i%1000==0)
			sleep(1);
		dhmp_free(addr[i]);
	}
	
	dhmp_client_destroy();
	task_time_diff_ns = ((task_time_end.tv_sec * 1000000000) + task_time_end.tv_nsec) -
                        ((task_time_start.tv_sec * 1000000000) + task_time_start.tv_nsec);
  	printf("runtime %lf\n", (double)task_time_diff_ns/1000000);
	return 0;
}



