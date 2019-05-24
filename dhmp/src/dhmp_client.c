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

struct dhmp_client *client=NULL;
int rdelay,wdelay,knum;

static struct dhmp_transport* dhmp_node_select()
{
	int i;
	
	for(i=0; i<DHMP_SERVER_NODE_NUM; i++)
	{
		if(client->fifo_node_index>=DHMP_SERVER_NODE_NUM)
			client->fifo_node_index=0;

		if(client->connect_trans[client->fifo_node_index]!=NULL &&
			(client->connect_trans[client->fifo_node_index]->trans_state==
				DHMP_TRANSPORT_STATE_CONNECTED))
		{
			++client->fifo_node_index;
			return client->connect_trans[client->fifo_node_index-1];
		}

		++client->fifo_node_index;
	}
	
	return NULL;
}

struct dhmp_transport* dhmp_get_trans_from_addr(void *dhmp_addr)
{
	long long node_index=(long long)dhmp_addr;
	node_index=node_index>>48;
	return client->connect_trans[node_index];
}

void *dhmp_malloc(size_t length)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_malloc_work malloc_work;
	struct dhmp_work *work;
	struct dhmp_addr_info *addr_info;
	
	if(length<=0)
	{
		ERROR_LOG("length is error.");
		goto out;
	}

	/*select which node to alloc nvm memory*/
	rdma_trans=dhmp_node_select();
	if(!rdma_trans)
	{
		ERROR_LOG("don't exist remote server.");
		goto out;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("allocate memory error.");
		goto out;
	}
	
	addr_info=malloc(sizeof(struct dhmp_addr_info));
	if(!addr_info)
	{
		ERROR_LOG("allocate memory error.");
		goto out_work;
	}
	addr_info->nvm_mr.length=0;
	addr_info->dram_mr.addr=NULL;
	
	malloc_work.addr_info=addr_info;
	malloc_work.rdma_trans=rdma_trans;
	malloc_work.length=length;
	malloc_work.done_flag=false;

	work->work_type=DHMP_WORK_MALLOC;
	work->work_data=&malloc_work;

	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!malloc_work.done_flag);

	free(work);
	
	return malloc_work.res_addr;

out_work:
	free(work);
out:
	return NULL;
}

void dhmp_free(void *dhmp_addr)
{
	struct dhmp_free_work free_work;
	struct dhmp_work *work;
	struct dhmp_transport *rdma_trans;
	
	if(dhmp_addr==NULL)
	{
		ERROR_LOG("dhmp address is NULL");
		return ;
	}

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return ;
	}
	
	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("allocate memory error.");
		return ;
	}

	free_work.rdma_trans=rdma_trans;
	free_work.dhmp_addr=dhmp_addr;
	free_work.done_flag=false;
	
	work->work_type=DHMP_WORK_FREE;
	work->work_data=&free_work;

	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!free_work.done_flag);

	free(work);
}

int dhmp_read(void *dhmp_addr, void * local_buf, size_t count)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_rw_work rwork;
	struct dhmp_work *work;
	
	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);;
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("allocate memory error.");
		return -1;
	}
	
	rwork.done_flag=false;
	rwork.length=count;
	rwork.local_addr=local_buf;
	rwork.dhmp_addr=dhmp_addr;
	rwork.rdma_trans=rdma_trans;
	
	work->work_type=DHMP_WORK_READ;
	work->work_data=&rwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);

	while(!rwork.done_flag);

	free(work);
	
	return 0;
}

int dhmp_write(void *dhmp_addr, void * local_buf, size_t count)
{
	struct dhmp_transport *rdma_trans=NULL;
	struct dhmp_rw_work wwork;
	struct dhmp_work *work;

	rdma_trans=dhmp_get_trans_from_addr(dhmp_addr);
	if(!rdma_trans||rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
	{
		ERROR_LOG("rdma connection error.");
		return -1;
	}

	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("alloc memory error.");
		return -1;
	}
	wwork.done_flag=false;
	wwork.length=count;
	wwork.local_addr=local_buf;
	wwork.dhmp_addr=dhmp_addr;
	wwork.rdma_trans=rdma_trans;
			
	work->work_type=DHMP_WORK_WRITE;
	work->work_data=&wwork;
	
	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!wwork.done_flag);

	free(work);
	
	return 0;
}

struct dhmp_device *dhmp_get_dev_from_client()
{
	struct dhmp_device *res_dev_ptr=NULL;
	if(!list_empty(&client->dev_list))
	{
		res_dev_ptr=list_first_entry(&client->dev_list,
									struct dhmp_device,
									dev_entry);
	}
		
	return res_dev_ptr;
}

void *dhmp_poll_ht_thread(void *data)
{
	uint64_t exp=0;
	struct dhmp_work *work;
	
	while(1)
	{
		read(client->poll_ht_fd, &exp, sizeof(uint64_t));
		work=malloc(sizeof(struct dhmp_work));
		work->work_type=DHMP_WORK_POLL;
		work->work_data=NULL;
		
		pthread_mutex_lock(&client->mutex_work_list);
		list_add_tail(&work->work_entry, &client->work_list);
		pthread_mutex_unlock(&client->mutex_work_list);
	}
	
	return NULL;
}

void dhmp_client_init()
{
	int i;
	struct itimerspec poll_its;
	
	client=(struct dhmp_client *)malloc(sizeof(struct dhmp_client));
	if(!client)
	{
		ERROR_LOG("alloc memory error.");
		return ;
	}

	dhmp_hash_init();
	dhmp_config_init(&client->config, true);
	dhmp_context_init(&client->ctx);
	rdelay=client->config.simu_infos[0].rdelay;
	wdelay=client->config.simu_infos[0].wdelay;
	knum=client->config.simu_infos[0].knum;
	
	/*init list about rdma device*/
	INIT_LIST_HEAD(&client->dev_list);
	dhmp_dev_list_init(&client->dev_list);

	/*init FIFO node select algorithm*/
	client->fifo_node_index=0;


	/*init the addr hash table of client*/
	for(i=0;i<DHMP_CLIENT_HT_SIZE;i++)
	{
		INIT_HLIST_HEAD(&client->addr_info_ht[i]);
	}

	
	/*init the structure about send mr list */
	pthread_mutex_init(&client->mutex_send_mr_list, NULL);
	INIT_LIST_HEAD(&client->send_mr_list);

	
	/*init normal connection*/
	memset(client->connect_trans, 0, DHMP_SERVER_NODE_NUM*
										sizeof(struct dhmp_transport*));
	for(i=0;i<client->config.nets_cnt;i++)
	{
		INFO_LOG("create the [%d]-th normal transport.",i);
		client->connect_trans[i]=dhmp_transport_create(&client->ctx, 
														dhmp_get_dev_from_client(),
														false,
														false);
		if(!client->connect_trans[i])
		{
			ERROR_LOG("create the [%d]-th transport error.",i);
			continue;
		}
		client->connect_trans[i]->node_id=i;
		dhmp_transport_connect(client->connect_trans[i],
							client->config.net_infos[i].addr,
							client->config.net_infos[i].port);
	}

	for(i=0;i<client->config.nets_cnt;i++)
	{
		if(client->connect_trans[i]==NULL)
			continue;
		while(client->connect_trans[i]->trans_state<DHMP_TRANSPORT_STATE_CONNECTED);
	}

	/*init the poll connection*/
	memset(client->poll_trans, 0, DHMP_SERVER_NODE_NUM*
										sizeof(struct dhmp_transport*));
	for(i=0;i<client->config.nets_cnt;i++)
	{
		INFO_LOG("create the [%d]-th poll transport.",i);
		client->poll_trans[i]=dhmp_transport_create(&client->ctx, 
													dhmp_get_dev_from_client(),
													false, true);
		
		if(!client->poll_trans[i])
		{
			ERROR_LOG("create the [%d]-th transport error.",i);
			continue;
		}
		client->poll_trans[i]->node_id=i;
		dhmp_transport_connect(client->poll_trans[i],
							client->config.net_infos[i].addr,
							client->config.net_infos[i].port);
	}
	
	for(i=0;i<client->config.nets_cnt;i++)
	{
		if(client->poll_trans[i]==NULL)
			continue;
		while(client->poll_trans[i]->trans_state<DHMP_TRANSPORT_STATE_CONNECTED);
	}

	
	/*threshhold init*/
	for(i=0;i<DHMP_SERVER_NODE_NUM;i++)
	{
		client->access_dram_num[i]=0;
		client->threshold[i]=2;
		client->per_benefit[i]=0.0;
		client->per_benefit_max[i]=0.2;
		client->dram_threshold_policy[i]=true;
		client->req_dram_size[i]=0;
		client->res_dram_size[i]=0;
		client->pre_hit_ratio[i]=-1.0;
		client->pre_hit_ratio_max[i]=0.08;
	}
	
	
	/*poll cycle init param*/
	client->pre_average_size=DHMP_DEFAULT_SIZE;
	client->access_region_size=0;
	client->access_total_num=0;

	
	/*create timer poll hash table*/
	client->poll_interval.tv_sec=0;
	client->poll_interval.tv_nsec=DHMP_DEFAULT_POLL_TIME;
	
	poll_its.it_value.tv_sec = client->poll_interval.tv_sec;
	poll_its.it_value.tv_nsec = client->poll_interval.tv_nsec;
	poll_its.it_interval.tv_sec = 0;
	poll_its.it_interval.tv_nsec = 0;
	
#ifdef DHMP_CACHE_POLICY
	client->poll_ht_fd=dhmp_timerfd_create(&poll_its);
	pthread_create(&client->poll_ht_thread, NULL, dhmp_poll_ht_thread, (void*)client);
#endif
	
	/*init the structure about work thread*/
	pthread_mutex_init(&client->mutex_work_list, NULL);
	INIT_LIST_HEAD(&client->work_list);
	pthread_create(&client->work_thread, NULL, dhmp_work_handle_thread, (void*)client);
}

static void dhmp_close_connection(struct dhmp_transport *rdma_trans)
{
	struct dhmp_close_work close_work;
	struct dhmp_work *work;

	if(rdma_trans==NULL ||
		rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
		return ;
	
	work=malloc(sizeof(struct dhmp_work));
	if(!work)
	{
		ERROR_LOG("allocate memory error.");
		return ;
	}

	close_work.rdma_trans=rdma_trans;
	close_work.done_flag=false;
	
	work->work_type=DHMP_WORK_CLOSE;
	work->work_data=&close_work;

	pthread_mutex_lock(&client->mutex_work_list);
	list_add_tail(&work->work_entry, &client->work_list);
	pthread_mutex_unlock(&client->mutex_work_list);
	
	while(!close_work.done_flag);

	free(work);
}

void dhmp_client_destroy()
{
	int i;
	INFO_LOG("send all disconnect start.");
	for(i=0;i<client->config.nets_cnt;i++)
	{
		dhmp_close_connection(client->connect_trans[i]);
	}
	
	for(i=0;i<client->config.nets_cnt;i++)
	{
		dhmp_close_connection(client->poll_trans[i]);
	}
	
	for(i=0;i<client->config.nets_cnt;i++)
	{
		if(client->connect_trans[i]==NULL)
			continue;
		while(client->connect_trans[i]->trans_state==DHMP_TRANSPORT_STATE_CONNECTED);
	}

	for(i=0;i<client->config.nets_cnt;i++)
	{
		if(client->poll_trans[i]==NULL)
			continue;
		while(client->poll_trans[i]->trans_state==DHMP_TRANSPORT_STATE_CONNECTED);
	}

	client->ctx.stop=true;
	
	INFO_LOG("client destroy start.");
	pthread_join(client->ctx.epoll_thread, NULL);
	INFO_LOG("client destroy end.");
	
	free(client);
}

