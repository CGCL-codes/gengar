#include <stdio.h>

#include "dhmp.h"

#define R 30000
#define NUM_SIZE 300000
const double A = 1.1;  //定义参数A>1的浮点数, 后来测试小于1的,似乎也可以,倾斜指数）
const double C = 1.0;  //这个C是不重要的,一般取1, 可以看到下面计算中分子分母可以约掉这个C

double pf[R]; //值为0~1之间, 是单个f(r)的累加值
int rand_num[NUM_SIZE]={0};

void generate()
{
    int i;
    double sum = 0.0;

    for (i = 0; i < R; i++)
        sum += C/pow((double)(i+2), A);

    for (i = 0; i < R; i++)
    {
        if (i == 0)
            pf[i] = C/pow((double)(i+2), A)/sum;
        else
            pf[i] = pf[i-1] + C/pow((double)(i+2), A)/sum;
    }
}

void pick()
{
	int i, index;

    generate();

    srand(time(0));
    //产生n个数
    for ( i= 0; i < NUM_SIZE; i++)
    {
        index = 0;
        double data = (double)rand()/RAND_MAX;  //生成一个0~1的数
        while (index<R-1&&data > pf[index])   //找索引,直到找到一个比他小的值,那么对应的index就是随机数了
            index++;
		rand_num[i]=index;
    }
}

int main(int argc,char *argv[])
{
	void *addr;
	int i,size=65536,readnum,writenum;
	char *str;
	struct timespec task_time_start, task_time_end;
	unsigned long task_time_diff_ns;
	
	if(argc<4)
	{
		printf("input param error. input:<filname> <size> <readnum> <writenum>\n");
		return -1;
	}
	else
	{
		size=atoi(argv[1]);
		readnum=atoi(argv[2]);
		writenum=atoi(argv[3]);
	}

	pick();
	
	str=malloc(size);
	if(!str){
		printf("alloc mem error");
		return -1;
	}
	snprintf(str, size, "hello world hello world hello world hello world hello world");
	
	dhmp_client_init();
	
	addr=dhmp_malloc(size);
	
	clock_gettime(CLOCK_MONOTONIC, &task_time_start);
	
	for(i=0;i<readnum;i++)
		dhmp_read(addr, str, size);
	
	for(i=0;i<writenum;i++)
		dhmp_write(addr, str, size);
	
	
	clock_gettime(CLOCK_MONOTONIC, &task_time_end);

	dhmp_free(addr);
	
	dhmp_client_destroy();
	task_time_diff_ns = ((task_time_end.tv_sec * 1000000000) + task_time_end.tv_nsec) -
                        ((task_time_start.tv_sec * 1000000000) + task_time_start.tv_nsec);

	printf("size %d readnum %d writenum %d ",size,readnum,writenum);
	printf("runtime %lf\n", (double)task_time_diff_ns/1000000);
	return 0;
}



