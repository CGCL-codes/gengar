#include <stdio.h>

#include "dhmp.h"

int main(int argc,char *argv[])
{
	void *addr[1000];
	int i,size=8388608,rnum,readnum,writenum;
	char *str;
	
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
	
	for(i=0;i<1000;i++)
	{
		if(i%50==0)
			sleep(1);
		addr[i]=dhmp_malloc(size);
	}
	
	for(i=0;i<readnum;i++)
	{
		rnum=rand()%1000;
		dhmp_read(addr[rnum], str, size);
	}
	
	for(i=0;i<writenum;i++)
	{
		rnum=rand()%1000;
		dhmp_write(addr[rnum], str, size);
	}

	for(i=0;i<1000;i++)
	{
		if(i%50==0)
			sleep(1);
		dhmp_free(addr[i]);
	}
	dhmp_client_destroy();
	
	return 0;
}


