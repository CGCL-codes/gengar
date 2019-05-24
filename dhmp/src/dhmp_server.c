#include "dhmp.h"
#include "dhmp_log.h"
#include "dhmp_hash.h"
#include "dhmp_config.h"
#include "dhmp_context.h"
#include "dhmp_dev.h"
#include "dhmp_transport.h"
#include "dhmp_task.h"
#include "dhmp_timerfd.h"
#include "dhmp_poll.h"
#include "dhmp_work.h"
#include "dhmp_watcher.h"
#include "dhmp_client.h"
#include "dhmp_server.h"
//4KB:4096,8192,16384,32768,65536,131072,262144
//8KB:8192,16384,32768,65536,131072,262144,524288
//16KB:16384,32768,65536,131072,262144,524288
//32KB:32768,65536,131072,262144,524288,1048576
//64KB:65536,131072,262144,524288,1048576,2097152
//128KB:131072,262144,524288,1048576,2097152,4194304
//256KB:262144,524288,1048576,2097152,4194304,8388608
//512KB:524288,1048576,2097152,4194304,8388608,16777216
//1MB:1048576,2097152,4194304,8388608,16777216,33554432
//2MB:2097152,4194304,8388608,16777216,33554432,67108864
//3MB:3145728,6291456,12582912,25165824,50331648,100663296
//4MB:4194304,8388608,16777216,33554432,67108864,134217728
//5MB:5242880,10485760,20971520,41943040,83886080,167772160
//6MB:6291456,12582912,25165824,50331648,100663296,201326592
//8MB:8388608,16777216,33554432,67108864,134217728,268435456
//32MB:33554432,67108864,134217728,268435456,536870912
//64MB:67108864,134217728,268435456,536870912,1073741824,2147483648


/*BUDDY_NUM_SUM = the total num of buddy num array*/
#define BUDDY_NUM_SUM 6
const int buddy_num[MAX_ORDER]={2,1,1,1,1};
const size_t buddy_size[MAX_ORDER]=
			{65536,131072,262144,524288,1048576};

struct dhmp_server *server=NULL;

int dhmp_hash_in_server(void *nvm_addr)
{
	uint32_t key;
	int index;

	key=hash(&nvm_addr,sizeof(void*));
	index=((key%DHMP_DRAM_HT_SIZE)+DHMP_DRAM_HT_SIZE)%DHMP_DRAM_HT_SIZE;

	return index;
}

/**
 *	dhmp_get_dev_from_server:get the dev_ptr from dev_list of server.
 */
struct dhmp_device *dhmp_get_dev_from_server()
{
	struct dhmp_device *res_dev_ptr=NULL;
	if(!list_empty(&server->dev_list))
	{
		res_dev_ptr=list_first_entry(&server->dev_list,
									struct dhmp_device,
									dev_entry);
	}
		
	return res_dev_ptr;
}

/**
 *	dhmp_buddy_system_build:build the buddy system in area
 */
bool dhmp_buddy_system_build(struct dhmp_area *area)
{
	struct dhmp_free_block *free_blk[BUDDY_NUM_SUM];
	int i,j,k,size=buddy_size[0],total=0;

	for(k=0;k<BUDDY_NUM_SUM;k++)
	{
		free_blk[k]=malloc(sizeof(struct dhmp_free_block));
		if(!free_blk[k])
		{
			ERROR_LOG("allocate memory error.");
			goto out_free_blk_array;
		}
	}
	
	for(i=0,k=0;i<MAX_ORDER;i++)
	{
		INIT_LIST_HEAD(&area->block_head[i].free_block_list);
		area->block_head[i].nr_free=buddy_num[i];
		
		for(j=0;j<buddy_num[i];j++)
		{
			free_blk[k]->addr=area->mr->addr+total;
			free_blk[k]->size=size;
			free_blk[k]->mr=area->mr;
			list_add_tail(&free_blk[k]->free_block_entry,
						&area->block_head[i].free_block_list);
			total+=size;
			INFO_LOG("i %d k %d addr %p",i,k,free_blk[k]->addr);
			k++;
		}
		
		size*=2;
	}

	return true;
	
out_free_blk_array:
	for(k=0;k<BUDDY_NUM_SUM;k++)
	{
		if(free_blk[k])
			free(free_blk[k]);
		else
			break;
	}
	return false;
}

struct dhmp_area *dhmp_area_create(bool has_buddy_sys,size_t length)
{
	void *addr=NULL;
	struct dhmp_area *area=NULL;
	struct ibv_mr *mr;
	struct dhmp_device *dev;
	bool res;
	
	/*nvm memory*/
	addr=malloc(length);
	if(!addr)
	{
		ERROR_LOG("allocate nvm memory error.");
		return NULL;
	}

	dev=dhmp_get_dev_from_server();
	mr=ibv_reg_mr(dev->pd,
				addr, length, 
				IBV_ACCESS_LOCAL_WRITE|
				IBV_ACCESS_REMOTE_READ|
				IBV_ACCESS_REMOTE_WRITE);
	if(!mr)
	{
		ERROR_LOG("ib register mr error.");
		goto out_addr;
	}

	area=malloc(sizeof(struct dhmp_area));
	if(!area)
	{
		ERROR_LOG("allocate memory error.");
		goto out_mr;
	}

	area->mr=mr;
	if(has_buddy_sys)
	{
		area->max_index=MAX_ORDER-1;
		res=dhmp_buddy_system_build(area);
		if(!res)
			goto out_area;
		list_add(&area->area_entry, &server->area_list);
	}
	else
	{
		list_add(&area->area_entry, &server->more_area_list);
		area->max_index=-2;	
	}
	
	return area;

out_area:
	free(area);
out_mr:
	ibv_dereg_mr(mr);
out_addr:
	free(addr);

	return NULL;
}

void dhmp_server_init()
{
	int i,err=0;
	
	server=(struct dhmp_server *)malloc(sizeof(struct dhmp_server));
	if(!server)
	{
		ERROR_LOG("allocate memory error.");
		return ;
	}
	
	dhmp_hash_init();
	dhmp_config_init(&server->config, false);
	dhmp_context_init(&server->ctx);

	/*init client transport list*/
	server->cur_connections=0;
	pthread_mutex_init(&server->mutex_client_list, NULL);
	INIT_LIST_HEAD(&server->client_list);
	
	/*init list about rdma device*/
	INIT_LIST_HEAD(&server->dev_list);
	dhmp_dev_list_init(&server->dev_list);

	/*init the structure about memory count*/
	/*get dram total size, get nvm total size*/
	server->dram_total_size=numa_node_size(0, NULL);
	server->nvm_total_size=numa_node_size(1, NULL);

	server->dram_used_size=server->nvm_used_size=0;
	INFO_LOG("server dram total size %ld",server->dram_total_size);
	INFO_LOG("server nvm total size %ld",server->nvm_total_size);
	
	/*init dram cache*/
	for(i=0; i<DHMP_DRAM_HT_SIZE; i++)
		INIT_HLIST_HEAD(&server->dram_ht[i]);
	
	server->listen_trans=dhmp_transport_create(&server->ctx,
											dhmp_get_dev_from_server(),
											true, false);
	if(!server->listen_trans)
	{
		ERROR_LOG("create rdma transport error.");
		exit(-1);
	}

	err=dhmp_transport_listen(server->listen_trans,
					server->config.net_infos[server->config.curnet_id].port);
	if(err)
		exit(- 1);

	/*create one area and init area list*/	
	INIT_LIST_HEAD(&server->area_list);
	INIT_LIST_HEAD(&server->more_area_list);
	server->cur_area=dhmp_area_create(true, SINGLE_AREA_SIZE);
	
}

void dhmp_server_destroy()
{
	INFO_LOG("server destroy start.");
	pthread_join(server->ctx.epoll_thread, NULL);
	INFO_LOG("server destroy end.");
	free(server);
}
