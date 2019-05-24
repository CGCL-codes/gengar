#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>

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

#ifdef DHMP_MR_REUSE_POLICY
#define RDMA_SEND_THREASHOLD 2097152
#endif

/*declare static function in here*/
static void dhmp_post_recv(struct dhmp_transport* rdma_trans, void *addr);
static void dhmp_post_all_recv(struct dhmp_transport *rdma_trans);
bool dhmp_destroy_dram_entry(void *nvm_addr);

/**
 *	about watcher handle function
 */
void dhmp_inform_watcher_func(struct dhmp_transport *watcher_trans)
{
	struct dhmp_app_mem_info app_mem_infos[DHMP_MAX_CLIENT_NUM];
	struct dhmp_transport *rdma_trans;
	struct dhmp_msg msg;
	int index=0;

	if(watcher_trans==NULL || watcher_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
		return ;
	
	pthread_mutex_lock(&server->mutex_client_list);
	list_for_each_entry(rdma_trans, &server->client_list, client_entry)
	{
		if((rdma_trans->link_trans!=NULL)&&(!rdma_trans->is_poll_qp))
		{
			app_mem_infos[index].dram_used_size=rdma_trans->dram_used_size+
												rdma_trans->link_trans->dram_used_size;
			app_mem_infos[index].nvm_used_size=rdma_trans->nvm_used_size+
												rdma_trans->link_trans->nvm_used_size;
			index++;
		}
	}
	pthread_mutex_unlock(&server->mutex_client_list);
	
	msg.msg_type=DHMP_MSG_MEM_CHANGE;
	msg.data_size=index*sizeof(struct dhmp_app_mem_info);
	msg.data=app_mem_infos;
	
	dhmp_post_send(watcher_trans, &msg);
}

void dhmp_mem_change_handle(struct dhmp_transport *rdma_trans,
									struct dhmp_msg* msg)
{
	struct dhmp_app_mem_info *app_mem_head;
	int i,node_index=0,len=0;
	
	app_mem_head=(struct dhmp_app_mem_info*)msg->data;
	len=msg->data_size/sizeof(struct dhmp_app_mem_info);
		
	while(node_index<DHMP_SERVER_NODE_NUM)
	{
		if(rdma_trans==watcher->connect_trans[node_index])
			break;
		node_index++;
	}

	for(i=0; i<len; i++)
	{
		watcher->apps_info[node_index][i].dram_used_size=app_mem_head->dram_used_size;
		watcher->apps_info[node_index][i].nvm_used_size=app_mem_head->nvm_used_size;
		app_mem_head++;
	}
	watcher->cur_app_num=len;
}

/**
 *	return the work completion operation code string.
 */
const char* dhmp_wc_opcode_str(enum ibv_wc_opcode opcode)
{
	switch(opcode)
	{
		case IBV_WC_SEND:
			return "IBV_WC_SEND";
		case IBV_WC_RDMA_WRITE:
			return "IBV_WC_RDMA_WRITE";
		case IBV_WC_RDMA_READ:
			return "IBV_WC_RDMA_READ";
		case IBV_WC_COMP_SWAP:
			return "IBV_WC_COMP_SWAP";
		case IBV_WC_FETCH_ADD:
			return "IBV_WC_FETCH_ADD";
		case IBV_WC_BIND_MW:
			return "IBV_WC_BIND_MW";
		case IBV_WC_RECV:
			return "IBV_WC_RECV";
		case IBV_WC_RECV_RDMA_WITH_IMM:
			return "IBV_WC_RECV_RDMA_WITH_IMM";
		default:
			return "IBV_WC_UNKNOWN";
	};
}

/**
 *	below functions about malloc memory in dhmp_server
 */
static struct dhmp_free_block* dhmp_get_free_block(struct dhmp_area* area,
														int index)
{
	struct dhmp_free_block *res,*left_blk,*right_blk;
	int i;

	res=left_blk=right_blk=NULL;
retry:
	for(i=index; i<MAX_ORDER; i++)
	{
		if(!list_empty(&area->block_head[i].free_block_list))
			break;
	}

	if(i==MAX_ORDER)
	{
		ERROR_LOG("don't exist enough large free block.");
		res=NULL;
		goto out;
	}

	if(i!=index)
	{
		right_blk=malloc(sizeof(struct dhmp_free_block));
		if(!right_blk)
		{
			ERROR_LOG("allocate memory error.");
			goto out;
		}

		left_blk=list_entry(area->block_head[i].free_block_list.next,
						struct dhmp_free_block, free_block_entry);
		list_del(&left_blk->free_block_entry);
		left_blk->size=left_blk->size/2;

		right_blk->mr=left_blk->mr;
		right_blk->size=left_blk->size;
		right_blk->addr=left_blk->addr+left_blk->size;

		list_add(&right_blk->free_block_entry, 
				&area->block_head[i-1].free_block_list);
		list_add(&left_blk->free_block_entry, 
				&area->block_head[i-1].free_block_list);
		
		area->block_head[i].nr_free-=1;
		area->block_head[i-1].nr_free+=2;
		goto retry;
	}
	else
	{
		res=list_entry(area->block_head[i].free_block_list.next,
						struct dhmp_free_block, free_block_entry);
		list_del(&res->free_block_entry);
		area->block_head[i].nr_free-=1;
	}

	for(i=area->max_index; i>=0; i--)
	{
		if(!list_empty(&area->block_head[i].free_block_list))
			break;
	}

	area->max_index=i;
	
out:
	return res;
}

static bool dhmp_malloc_one_block(struct dhmp_msg* msg,
										struct dhmp_mc_response* response)
{
	struct dhmp_free_block* free_blk=NULL;
	struct dhmp_area* area=NULL;
	int index=0;

	for(index=0; index<MAX_ORDER; index++)
		if(response->req_info.req_size<=buddy_size[index])
			break;
	
	if(server->cur_area->max_index>=index)
		area=server->cur_area;
	else
	{
		list_for_each_entry(area, &server->area_list, area_entry)
		{
			if(area->max_index>=index)
			{
				server->cur_area=area;
				break;
			}
		}
	}
	
	if(&area->area_entry == &(server->area_list))
	{
		area=dhmp_area_create(true, SINGLE_AREA_SIZE);
		if(!area)
		{
			ERROR_LOG ( "allocate area memory error." );
			goto out;
		}
		server->cur_area=area;
	}
	
	free_blk=dhmp_get_free_block(area, index);
	if(!free_blk)
	{
		ERROR_LOG("fetch free block error.");
		goto out;
	}

	memcpy(&response->mr, free_blk->mr, sizeof(struct ibv_mr));
	response->mr.addr=free_blk->addr;
	response->mr.length=free_blk->size;

	DEBUG_LOG("malloc addr %p lkey %ld length is %d",
			free_blk->addr, free_blk->mr->lkey, free_blk->mr->length );

	snprintf(free_blk->addr, response->req_info.req_size, "nvmhhhhhhhh%p", free_blk);

	free(free_blk);
	return true;

out:
	return false;
}


static bool dhmp_malloc_more_area(struct dhmp_msg* msg, 
										struct dhmp_mc_response* response_msg,
										size_t length )
{
	struct dhmp_area *area;

	area=dhmp_area_create(false, length);
	if(!area)
	{
		ERROR_LOG("allocate one area error.");
		return false;
	}
	
	memcpy(&response_msg->mr, area->mr, sizeof(struct ibv_mr));
	
	INFO_LOG("malloc addr %p lkey %ld",
			response_msg->mr.addr, response_msg->mr.lkey);
	
	snprintf(response_msg->mr.addr,
			response_msg->req_info.req_size, 
			"welcomebj%p", area);

	return true;
}

static void dhmp_malloc_request_handler(struct dhmp_transport* rdma_trans,
												struct dhmp_msg* msg)
{
	struct dhmp_mc_response response;
	struct dhmp_msg res_msg;
	bool res=true;
	
	memcpy ( &response.req_info, msg->data, sizeof(struct dhmp_mc_request));
	INFO_LOG ( "client req size %d",response.req_info.req_size);

	
	if(response.req_info.req_size <= buddy_size[MAX_ORDER-1])
	{
		//DEBUG_LOG ( "alloc buddy block" );
		res=dhmp_malloc_one_block(msg, &response);
	}
	else if(response.req_info.req_size <= SINGLE_AREA_SIZE)
	{
		//DEBUG_LOG ( "alloc one area" );
		res=dhmp_malloc_more_area(msg, &response, SINGLE_AREA_SIZE);
	}
	else
	{
		//DEBUG_LOG ( "alloc more area" );
		res=dhmp_malloc_more_area(msg, &response, response.req_info.req_size);
	}

	if(!res)
		goto req_error;
	
	res_msg.msg_type=DHMP_MSG_MALLOC_RESPONSE;
	res_msg.data_size=sizeof(struct dhmp_mc_response);
	res_msg.data=&response;
	dhmp_post_send(rdma_trans, &res_msg);
	/*count the server memory,and inform the watcher*/
	rdma_trans->nvm_used_size+=response.mr.length;
	dhmp_inform_watcher_func(server->watcher_trans);
	return ;

req_error:
	/*transmit a message of DHMP_MSG_MALLOC_ERROR*/
	res_msg.msg_type=DHMP_MSG_MALLOC_ERROR;
	res_msg.data_size=sizeof(struct dhmp_mc_response);
	res_msg.data=&response;

	dhmp_post_send ( rdma_trans, &res_msg );

	return ;
}

static void dhmp_malloc_response_handler(struct dhmp_transport* rdma_trans,
													struct dhmp_msg* msg)
{
	struct dhmp_mc_response response_msg;
	struct dhmp_addr_info *addr_info;

	memcpy(&response_msg, msg->data, sizeof(struct dhmp_mc_response));
	addr_info=response_msg.req_info.addr_info;
	memcpy(&addr_info->nvm_mr, &response_msg.mr, sizeof(struct ibv_mr));
	addr_info->read_cnt=addr_info->write_cnt=0;
	DEBUG_LOG("response mr addr %p lkey %ld",
			addr_info->nvm_mr.addr, addr_info->nvm_mr.lkey);
}

static void dhmp_malloc_error_handler(struct dhmp_transport* rdma_trans, struct dhmp_msg* msg)
{
	struct dhmp_mc_response response_msg;
	struct dhmp_addr_info *addr_info;
	
	memcpy( &response_msg, msg->data, sizeof(struct dhmp_mc_response));
	addr_info=response_msg.req_info.addr_info;
	addr_info->nvm_mr.length=response_msg.req_info.req_size;
	addr_info->nvm_mr.addr=NULL;
}

/**
 *	below functions about free memory in dhmp_server
 */
static struct dhmp_free_block* dhmp_free_blk_create(void* addr,
															size_t size,
															struct ibv_mr* mr)
{
	struct dhmp_free_block* new_free_blk;

	new_free_blk=malloc(sizeof(struct dhmp_free_block));
	if(!new_free_blk)
	{
		ERROR_LOG ( "allocate memory error." );
		return NULL;
	}
	
	new_free_blk->addr=addr;
	new_free_blk->size=size;
	new_free_blk->mr=mr;
	return new_free_blk;
}

static bool dhmp_recycle_free_block(struct dhmp_area* area,
											void** addr_ptr, int index)
{
	bool left_dir=true;
	void* addr=*addr_ptr;
	struct list_head* free_list;
	struct dhmp_free_block *new_free_blk=NULL,*large_free_blk,*tmp;

	free_list=&area->block_head[index].free_block_list;
	if(list_empty(free_list))
	{
		new_free_blk=dhmp_free_blk_create(addr, buddy_size[index], area->mr);
		list_add(&new_free_blk->free_block_entry, free_list);
		return false;
	}

	/*can not merge up index+1*/
	if(index==MAX_ORDER-1)
	{
		list_for_each_entry(large_free_blk, free_list, free_block_entry)
		{
			if(addr<large_free_blk->addr)
				goto create_free_blk;
		}
		goto create_free_blk;
	}
	
	if((addr-area->mr->addr)%buddy_size[index+1]!=0)
		left_dir=false;

	list_for_each_entry(tmp, free_list, free_block_entry)
	{
		if((left_dir&&(addr+buddy_size[index]==tmp->addr))||
			(!left_dir&&(tmp->addr+buddy_size[index]==addr)))
		{
			list_del(&tmp->free_block_entry);
			*addr_ptr=min(addr, tmp->addr);
			return true;
		}
	}

	list_for_each_entry(large_free_blk, free_list, free_block_entry)
	{
		if(addr<large_free_blk->addr)
			break;
	}

create_free_blk:
	new_free_blk=dhmp_free_blk_create(addr, buddy_size[index], area->mr);
	list_add_tail(&new_free_blk->free_block_entry, &large_free_blk->free_block_entry);
	return false;
}

static void dhmp_free_one_block(struct ibv_mr* mr)
{
	struct dhmp_area* area;
	int i,index;
	bool res;
	struct dhmp_free_block* free_blk;
	
	DEBUG_LOG("free one block %p size %d", mr->addr, mr->length);

	list_for_each_entry(area, &server->area_list, area_entry)
	{
		if(mr->lkey==area->mr->lkey)
			break;
	}
	
	if((&area->area_entry) != (&server->area_list))
	{
		for(index=0; index<MAX_ORDER; index++)
			if(mr->length==buddy_size[index])
				break;
			
retry:
		res=dhmp_recycle_free_block(area, &mr->addr, index);
		if(res&&(index!=MAX_ORDER-1))
		{
			index+=1;
			goto retry;
		}
		
		for(i=MAX_ORDER-1;i>=0;i--)
		{
			if(!list_empty(&area->block_head[i].free_block_list))
			{
				area->max_index=i;
				break;
			}
		}
		
		for ( i=0; i<MAX_ORDER; i++ )
		{
			list_for_each_entry(free_blk,
							&area->block_head[i].free_block_list,
							free_block_entry)
			{
				DEBUG_LOG("Index %d addr %p",i,free_blk->addr);
			}
		}
		
	}
}

static void dhmp_free_one_area(struct ibv_mr* mr)
{
	struct dhmp_area* area;
	bool res;
	void *addr;
	
	DEBUG_LOG("free one area %p size %d",mr->addr,mr->length);
	
	list_for_each_entry(area, &server->more_area_list, area_entry)
	{
		if(mr->lkey==area->mr->lkey)
			break;
	}
	
	if((&area->area_entry) != (&server->area_list))
	{
		res=dhmp_buddy_system_build(area);
		if(res)
		{
			list_del(&area->area_entry);
			area->max_index=MAX_ORDER-1;
			list_add(&area->area_entry, &server->area_list);
		}
		else
		{
			list_del(&area->area_entry);
			addr=area->mr->addr;
			ibv_dereg_mr(area->mr);
			free(addr);
			free(area);
		}
	}
}

static void dhmp_free_more_area(struct ibv_mr* mr)
{
	struct dhmp_area* area;
	void *addr;
	
	DEBUG_LOG("free more area %p size %d", mr->addr, mr->length);
	
	list_for_each_entry(area, &server->more_area_list, area_entry)
	{
		if(mr->lkey==area->mr->lkey)
			break;
	}
	
	if((&area->area_entry) != (&server->area_list))
	{
		list_del(&area->area_entry);
		addr=area->mr->addr;
		ibv_dereg_mr(area->mr);
		free(addr);
		free(area);
	}
}

static void dhmp_free_request_handler(struct dhmp_transport* rdma_trans, struct dhmp_msg* msg_ptr)
{
	struct ibv_mr* mr;
	struct dhmp_free_request *request_msg_ptr;
	struct dhmp_free_response free_res_msg;
	struct dhmp_msg res_msg;
	
	request_msg_ptr=msg_ptr->data;
	mr=&request_msg_ptr->mr;

	rdma_trans->nvm_used_size-=mr->length;
	if(dhmp_destroy_dram_entry(mr->addr))
	{
		server->dram_used_size-=mr->length;
		rdma_trans->dram_used_size-=mr->length;
	}
	if(mr->length<=buddy_size[MAX_ORDER-1])
		dhmp_free_one_block(mr);
	else if(mr->length<=SINGLE_AREA_SIZE)
		dhmp_free_one_area(mr);
	else
		dhmp_free_more_area(mr);

	free_res_msg.addr_info=request_msg_ptr->addr_info;
	
	res_msg.msg_type=DHMP_MSG_FREE_RESPONSE;
	res_msg.data_size=sizeof(struct dhmp_free_response);
	res_msg.data=&free_res_msg;

	dhmp_post_send(rdma_trans, &res_msg);
	
	dhmp_inform_watcher_func(server->watcher_trans);
}

static void dhmp_free_response_handler(struct dhmp_transport* rdma_trans, struct dhmp_msg* msg)
{
	struct dhmp_free_response *response_msg_ptr;
	struct dhmp_addr_info *addr_info;

	response_msg_ptr=msg->data;
	addr_info=response_msg_ptr->addr_info;
	addr_info->nvm_mr.addr=NULL;
}

/**
 * DRAM cache operations
 */
static struct dhmp_dram_entry *dhmp_create_new_dram_entry(void *nvm_addr, size_t length)
{
	int index;
	void *dram_addr;
	struct dhmp_dram_entry *dram_entry;
	struct dhmp_device *server_dev;
	
	index=dhmp_hash_in_server(nvm_addr);
	dram_entry=malloc(sizeof(struct dhmp_dram_entry));
	if(!dram_entry)
	{
		ERROR_LOG("allocate memory error.");
		goto out;
	}
	dram_entry->nvm_addr=nvm_addr;
	dram_entry->length=length;

	/*alloc dram memory*/
	dram_addr=malloc(length);
	if(!dram_addr)
	{
		ERROR_LOG("allocate dram memory error.");
		goto out_dram_entry;
	}

	server_dev=dhmp_get_dev_from_server();
	dram_entry->dram_mr=ibv_reg_mr(server_dev->pd, dram_addr, length, 
				IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_READ|IBV_ACCESS_REMOTE_WRITE);
	if(!dram_entry->dram_mr)
	{
		ERROR_LOG("allocate dram mr error");
		goto out_dram_addr;
	}
	hlist_add_head(&dram_entry->dram_node, &server->dram_ht[index]);
	
	return dram_entry;

out_dram_addr:
	free(dram_addr);
	
out_dram_entry:
	free(dram_entry);

out:
	return NULL;
}

bool dhmp_destroy_dram_entry(void *nvm_addr)
{
	struct dhmp_dram_entry *dram_entry;
	void *dram_addr;
	int index;

	index=dhmp_hash_in_server(nvm_addr);
	if(hlist_empty(&server->dram_ht[index]))
		return false;
	else{
		hlist_for_each_entry(dram_entry, &server->dram_ht[index], dram_node)
		{
			if(dram_entry->nvm_addr==nvm_addr)
				break;
		}
	}

	if(dram_entry)
	{
		DEBUG_LOG("delete dram entry.");
		hlist_del(&dram_entry->dram_node);
		dram_addr=dram_entry->dram_mr->addr;
		if(dram_entry->length>0)
			memcpy(nvm_addr, dram_addr, dram_entry->length);
		ibv_dereg_mr(dram_entry->dram_mr);
		free(dram_addr);
		free(dram_entry);
		return true;
	}
	return false;
}

struct dhmp_transport *dhmp_get_connect_trans(struct dhmp_transport* poll_trans)
{
	struct dhmp_transport *connect_trans;
	int index;

	for(index=0; index<DHMP_SERVER_NODE_NUM; index++)
	{
		if(client->poll_trans[index]==poll_trans)
			break;
	}
	return client->connect_trans[index];
}

static void dhmp_apply_dram_request_handler(struct dhmp_transport* rdma_trans, struct dhmp_msg* msg)
{
	int i,length;
	struct dhmp_nvm_info *tmp_head;
	struct dhmp_dram_entry *dram_entry;
	struct dhmp_dram_info dram_info[DHMP_MAX_OBJ_NUM];
	struct dhmp_msg response_msg;
	long cur_dram_threshold;
	
	length=msg->data_size/sizeof(struct dhmp_nvm_info);
	tmp_head=(struct dhmp_nvm_info*)msg->data;

	for(i=0;i<length;i++)
	{
		/*new create dhmp_dram_entry into hash table*/
		DEBUG_LOG("dhmp_dram_event insert %p %d",tmp_head->nvm_addr, tmp_head->length);
		cur_dram_threshold=DHMP_SERVER_DRAM_TH;
		cur_dram_threshold=cur_dram_threshold*2/((long)server->cur_connections);
		if((rdma_trans->dram_used_size+tmp_head->length)>cur_dram_threshold)
		{
			INFO_LOG("--------------CACHED-------------");
			length=i;
			break;
		}

#ifdef DHMP_CACHE_POLICY
		dram_entry=dhmp_create_new_dram_entry(tmp_head->nvm_addr, tmp_head->length);
		if(dram_entry)
		{
			dram_info[i].nvm_addr=tmp_head->nvm_addr;
			memcpy(&dram_info[i].dram_mr,dram_entry->dram_mr,sizeof(struct ibv_mr));
			//memcpy(dram_info[i].dram_mr.addr, tmp_head->nvm_addr, tmp_head->length);
			snprintf(dram_info[i].dram_mr.addr, tmp_head->length, "drambuf%p", tmp_head->nvm_addr);
			rdma_trans->dram_used_size+=tmp_head->length;
			server->dram_used_size+=tmp_head->length;
		}
		else
		{
#endif
			dram_info[i].nvm_addr=tmp_head->nvm_addr;
			dram_info[i].dram_mr.addr=NULL;
			dram_info[i].dram_mr.length=0;
			
#ifdef DHMP_CACHE_POLICY
		}
#endif

		tmp_head++;
	}
	
	response_msg.msg_type=DHMP_MSG_APPLY_DRAM_RESPONSE;
	response_msg.data_size=length*sizeof(struct dhmp_dram_info);
	response_msg.data=dram_info;

	dhmp_post_send(rdma_trans, &response_msg);
	
	dhmp_inform_watcher_func(server->watcher_trans);
}

static void dhmp_apply_dram_response_handler(struct dhmp_transport* rdma_trans, struct dhmp_msg* msg)
{
	struct dhmp_dram_info *dram_info_head;
	struct dhmp_transport *connect_trans;
	struct dhmp_addr_info *addr_info;
	int i,length,index, node_index;
	void *dhmp_addr;

	for(node_index=0; node_index<DHMP_SERVER_NODE_NUM; node_index++)
	{
		if(rdma_trans==client->connect_trans[node_index])
			break;
	}
	
	dram_info_head=msg->data;
	length=msg->data_size/sizeof(struct dhmp_dram_info);
	connect_trans=dhmp_get_connect_trans(rdma_trans);
	
	for(i=0;i<length;i++)
	{
		DEBUG_LOG("dram addr %p lkey %ld",
				dram_info_head->dram_mr.addr, dram_info_head->dram_mr.lkey);
		
		dhmp_addr=dhmp_transfer_dhmp_addr(connect_trans, dram_info_head->nvm_addr);
		index=dhmp_hash_in_client(dhmp_addr);
		addr_info=dhmp_get_addr_info_from_ht(index, dhmp_addr);
		
		if(addr_info)
		{
			if(addr_info->dram_mr.addr!=NULL)
				ERROR_LOG("hahahahah");
			
			if(dram_info_head->dram_mr.addr==NULL)
			{
				addr_info->dram_mr.addr=NULL;
				addr_info->dram_mr.length=0;
			}
			else
			{
				client->res_dram_size[node_index]+=addr_info->nvm_mr.length;
				memcpy(&addr_info->dram_mr,
						&dram_info_head->dram_mr,
						sizeof(struct ibv_mr));
			}
		}
		else
			ERROR_LOG("addr info not find.");
		dram_info_head++;
	}

	--client->poll_num;
}

static void dhmp_clear_dram_request_handler(struct dhmp_transport* rdma_trans, struct dhmp_msg* msg)
{
	int i,length;
	struct dhmp_nvm_info *tmp_head;
	struct dhmp_nvm_info nvm_info[DHMP_MAX_OBJ_NUM];
	struct dhmp_msg response_msg;
	
	length=msg->data_size/sizeof(struct dhmp_nvm_info);
	tmp_head=(struct dhmp_nvm_info*)msg->data;
	
	for(i=0;i<length;i++)
	{
		nvm_info[i].nvm_addr=tmp_head->nvm_addr;
		nvm_info[i].length=tmp_head->length;
		
		dhmp_destroy_dram_entry(tmp_head->nvm_addr);
		rdma_trans->dram_used_size-=tmp_head->length;
		server->dram_used_size-=tmp_head->length;
		
		tmp_head++;
	}

	response_msg.msg_type=DHMP_MSG_CLEAR_DRAM_RESPONSE;
	response_msg.data_size=length*sizeof(struct dhmp_nvm_info);
	response_msg.data=nvm_info;

	dhmp_post_send(rdma_trans, &response_msg);
	
	dhmp_inform_watcher_func(server->watcher_trans);
}

static void dhmp_clear_dram_response_handler(struct dhmp_transport* rdma_trans, struct dhmp_msg* msg)
{
	struct dhmp_transport *connect_trans;
	struct dhmp_addr_info *addr_info;
	struct dhmp_nvm_info *tmp_head;
	int i, length, index;	
	void *dhmp_addr;
	
	length=msg->data_size/sizeof(struct dhmp_nvm_info);
	tmp_head=(struct dhmp_nvm_info*)msg->data;
	connect_trans=dhmp_get_connect_trans(rdma_trans);
	
	for(i=0;i<length;i++)
	{
		dhmp_addr=dhmp_transfer_dhmp_addr(connect_trans, tmp_head->nvm_addr);
		index=dhmp_hash_in_client(dhmp_addr);
		addr_info=dhmp_get_addr_info_from_ht(index, dhmp_addr);

		if(addr_info)
		{
			addr_info->dram_mr.addr=NULL;
			addr_info->dram_mr.length=0;
		}
		else
			ERROR_LOG("dhmp not find addr info");
		tmp_head++;
	}

	--client->poll_num;
}

void dhmp_server_info_request_handler(struct dhmp_transport* rdma_trans, struct dhmp_msg* msg)
{
	struct dhmp_server_mem_info server_mem_info;
	struct dhmp_msg response_msg;

	server_mem_info.dram_total_size=server->dram_total_size;
	server_mem_info.nvm_total_size=server->nvm_total_size;
	
	response_msg.msg_type=DHMP_MSG_SERVER_INFO_RESPONSE;
	response_msg.data_size=sizeof(struct dhmp_server_mem_info);
	response_msg.data=&server_mem_info;

	dhmp_post_send(rdma_trans, &response_msg);
}

void dhmp_server_info_response_handler(struct dhmp_transport* rdma_trans, struct dhmp_msg* msg)
{
	struct dhmp_server_mem_info *server_mem_info;
	int node_index;

	server_mem_info=(struct dhmp_server_mem_info*)msg->data;

	for(node_index=0; node_index<DHMP_SERVER_NODE_NUM; node_index++)
	{
		if(watcher->connect_trans[node_index]==rdma_trans)
			break;
	}

	watcher->servers_info[node_index].dram_total_size=server_mem_info->dram_total_size;
	watcher->servers_info[node_index].nvm_total_size=server_mem_info->nvm_total_size;
}

/**
 *	dhmp_wc_recv_handler:handle the IBV_WC_RECV event
 */
static void dhmp_wc_recv_handler(struct dhmp_transport* rdma_trans,
										struct dhmp_msg* msg)
{
	switch(msg->msg_type)
	{
		case DHMP_MSG_MALLOC_REQUEST:
			dhmp_malloc_request_handler(rdma_trans, msg);
			break;
		case DHMP_MSG_MALLOC_RESPONSE:
			dhmp_malloc_response_handler(rdma_trans, msg);
			break;
		case DHMP_MSG_MALLOC_ERROR:
			dhmp_malloc_error_handler(rdma_trans, msg);
			break;
		case DHMP_MSG_FREE_REQUEST:
			dhmp_free_request_handler(rdma_trans, msg);
			break;
		case DHMP_MSG_FREE_RESPONSE:
			dhmp_free_response_handler(rdma_trans, msg);
			break;
		case DHMP_MSG_MEM_CHANGE:
			dhmp_mem_change_handle(rdma_trans, msg);
			break;
		case DHMP_MSG_APPLY_DRAM_REQUEST:
			dhmp_apply_dram_request_handler(rdma_trans, msg);
			break;
		case DHMP_MSG_APPLY_DRAM_RESPONSE:
			dhmp_apply_dram_response_handler(rdma_trans, msg);
			break;
		case DHMP_MSG_CLEAR_DRAM_REQUEST:
			dhmp_clear_dram_request_handler(rdma_trans, msg);
			break;
		case DHMP_MSG_CLEAR_DRAM_RESPONSE:
			dhmp_clear_dram_response_handler(rdma_trans, msg);
			break;
		case DHMP_MSG_SERVER_INFO_REQUEST:
			dhmp_server_info_request_handler(rdma_trans, msg);
			break;
		case DHMP_MSG_SERVER_INFO_RESPONSE:
			dhmp_server_info_response_handler(rdma_trans, msg);
			break;
		case DHMP_MSG_CLOSE_CONNECTION:
			rdma_disconnect(rdma_trans->cm_id);
			break;
	}
}

/**
 *	the success work completion handler function
 */
static void dhmp_wc_success_handler(struct ibv_wc* wc)
{
	struct dhmp_task *task_ptr;
	struct dhmp_transport *rdma_trans;
	struct dhmp_msg msg;
	
	task_ptr=(struct dhmp_task*)(uintptr_t)wc->wr_id;
	rdma_trans=task_ptr->rdma_trans;

	/*read the msg content from the task_ptr sge addr*/
	msg.msg_type=*(enum dhmp_msg_type*)task_ptr->sge.addr;
	msg.data_size=*(size_t*)(task_ptr->sge.addr+sizeof(enum dhmp_msg_type));
	msg.data=task_ptr->sge.addr+sizeof(enum dhmp_msg_type)+sizeof(size_t);
	
	switch(wc->opcode)
	{
		case IBV_WC_SEND:
			break;
		case IBV_WC_RECV:
			dhmp_wc_recv_handler(rdma_trans, &msg);
			dhmp_post_recv(rdma_trans, task_ptr->sge.addr);
			break;
		case IBV_WC_RDMA_WRITE:
#ifdef DHMP_MR_REUSE_POLICY
			if(task_ptr->sge.length <= RDMA_SEND_THREASHOLD)
			{
				pthread_mutex_lock(&client->mutex_send_mr_list);
				list_add(&task_ptr->smr->send_mr_entry, &client->send_mr_list);
				pthread_mutex_unlock(&client->mutex_send_mr_list);
			}
#endif
			task_ptr->addr_info->write_flag=false;
			task_ptr->done_flag=true;
			break;
		case IBV_WC_RDMA_READ:
			task_ptr->done_flag=true;
			break;
		default:
			ERROR_LOG("unknown opcode:%s",
			            dhmp_wc_opcode_str(wc->opcode));
			break;
	}
}

/**
 *	dhmp_wc_error_handler:handle the error work completion.
 */
static void dhmp_wc_error_handler(struct ibv_wc* wc)
{
	if(wc->status==IBV_WC_WR_FLUSH_ERR)
		INFO_LOG("work request flush");
	else
		ERROR_LOG("wc status is [%s]",
		            ibv_wc_status_str(wc->status));
}

/**
 *	dhmp_comp_channel_handler:create a completion channel handler
 *  note:set the following function to the cq handle work completion
 */
static void dhmp_comp_channel_handler(int fd, void* data)
{
	struct dhmp_cq* dcq =(struct dhmp_cq*) data;
	struct ibv_cq* cq;
	void* cq_ctx;
	struct ibv_wc wc;
	int err=0;

	err=ibv_get_cq_event(dcq->comp_channel, &cq, &cq_ctx);
	if(err)
	{
		ERROR_LOG("ibv get cq event error.");
		return ;
	}

	ibv_ack_cq_events(dcq->cq, 1);
	err=ibv_req_notify_cq(dcq->cq, 0);
	if(err)
	{
		ERROR_LOG("ibv req notify cq error.");
		return ;
	}

	while(ibv_poll_cq(dcq->cq, 1, &wc))
	{
		if(wc.status==IBV_WC_SUCCESS)
			dhmp_wc_success_handler(&wc);
		else
			dhmp_wc_error_handler(&wc);
	}
}

/*
 *	get the cq because send queue and receive queue need to link it
 */
static struct dhmp_cq* dhmp_cq_get(struct dhmp_device* device, struct dhmp_context* ctx)
{
	struct dhmp_cq* dcq;
	int retval,flags=0;

	dcq=(struct dhmp_cq*) calloc(1,sizeof(struct dhmp_cq));
	if(!dcq)
	{
		ERROR_LOG("allocate the memory of struct dhmp_cq error.");
		return NULL;
	}

	dcq->comp_channel=ibv_create_comp_channel(device->verbs);
	if(!dcq->comp_channel)
	{
		ERROR_LOG("rdma device %p create comp channel error.", device);
		goto cleanhcq;
	}

	flags=fcntl(dcq->comp_channel->fd, F_GETFL, 0);
	if(flags!=-1)
		flags=fcntl(dcq->comp_channel->fd, F_SETFL, flags|O_NONBLOCK);

	if(flags==-1)
	{
		ERROR_LOG("set hcq comp channel fd nonblock error.");
		goto cleanchannel;
	}

	dcq->ctx=ctx;
	retval=dhmp_context_add_event_fd(dcq->ctx,
									EPOLLIN,
									dcq->comp_channel->fd,
									dcq, dhmp_comp_channel_handler);
	if(retval)
	{
		ERROR_LOG("context add comp channel fd error.");
		goto cleanchannel;
	}

	dcq->cq=ibv_create_cq(device->verbs, 100000, dcq, dcq->comp_channel, 0);
	if(!dcq->cq)
	{
		ERROR_LOG("ibv create cq error.");
		goto cleaneventfd;
	}

	retval=ibv_req_notify_cq(dcq->cq, 0);
	if(retval)
	{
		ERROR_LOG("ibv req notify cq error.");
		goto cleaneventfd;
	}

	dcq->device=device;
	return dcq;

cleaneventfd:
	dhmp_context_del_event_fd(ctx, dcq->comp_channel->fd);

cleanchannel:
	ibv_destroy_comp_channel(dcq->comp_channel);

cleanhcq:
	free(dcq);

	return NULL;
}

/*
 *	create the qp resource for the RDMA connection
 */
static int dhmp_qp_create(struct dhmp_transport* rdma_trans)
{
	int retval=0;
	struct ibv_qp_init_attr qp_init_attr;
	struct dhmp_cq* dcq;

	dcq=dhmp_cq_get(rdma_trans->device, rdma_trans->ctx);
	if(!dcq)
	{
		ERROR_LOG("dhmp cq get error.");
		return -1;
	}

	memset(&qp_init_attr,0,sizeof(qp_init_attr));
	qp_init_attr.qp_context=rdma_trans;
	qp_init_attr.qp_type=IBV_QPT_RC;
	qp_init_attr.send_cq=dcq->cq;
	qp_init_attr.recv_cq=dcq->cq;

	qp_init_attr.cap.max_send_wr=15000;
	qp_init_attr.cap.max_send_sge=1;

	qp_init_attr.cap.max_recv_wr=15000;
	qp_init_attr.cap.max_recv_sge=1;

	retval=rdma_create_qp(rdma_trans->cm_id,
	                        rdma_trans->device->pd,
	                        &qp_init_attr);
	if(retval)
	{
		ERROR_LOG("rdma create qp error.");
		goto cleanhcq;
	}

	rdma_trans->qp=rdma_trans->cm_id->qp;
	rdma_trans->dcq=dcq;

	return retval;

cleanhcq:
	free(dcq);
	return retval;
}

static void dhmp_qp_release(struct dhmp_transport* rdma_trans)
{
	if(rdma_trans->qp)
	{
		ibv_destroy_qp(rdma_trans->qp);
		ibv_destroy_cq(rdma_trans->dcq->cq);
		dhmp_context_del_event_fd(rdma_trans->ctx,
								rdma_trans->dcq->comp_channel->fd);
		free(rdma_trans->dcq);
		rdma_trans->dcq=NULL;
	}
}


static int on_cm_addr_resolved(struct rdma_cm_event* event, struct dhmp_transport* rdma_trans)
{
	int retval=0;

	retval=rdma_resolve_route(rdma_trans->cm_id, ROUTE_RESOLVE_TIMEOUT);
	if(retval)
	{
		ERROR_LOG("RDMA resolve route error.");
		return retval;
	}

	return retval;
}

static int on_cm_route_resolved(struct rdma_cm_event* event, struct dhmp_transport* rdma_trans)
{
	struct rdma_conn_param conn_param;
	int i, retval=0;

	retval=dhmp_qp_create(rdma_trans);
	if(retval)
	{
		ERROR_LOG("hmr qp create error.");
		return retval;
	}

	memset(&conn_param, 0, sizeof(conn_param));
	conn_param.retry_count=100;
	conn_param.rnr_retry_count=200;
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;

	retval=rdma_connect(rdma_trans->cm_id, &conn_param);
	if(retval)
	{
		ERROR_LOG("rdma connect error.");
		goto cleanqp;
	}

	dhmp_post_all_recv(rdma_trans);
	return retval;

cleanqp:
	dhmp_qp_release(rdma_trans);
	rdma_trans->ctx->stop=1;
	rdma_trans->trans_state=DHMP_TRANSPORT_STATE_ERROR;
	return retval;
}

static struct dhmp_transport* dhmp_is_exist_connection(struct sockaddr_in *sock)
{
	char cur_ip[INET_ADDRSTRLEN], travers_ip[INET_ADDRSTRLEN];
	struct dhmp_transport *rdma_trans=NULL, *res_trans=NULL;
	struct in_addr in=sock->sin_addr;
	int cur_ip_len,travers_ip_len;
	
	inet_ntop(AF_INET, &(sock->sin_addr), cur_ip, sizeof(cur_ip));
	cur_ip_len=strlen(cur_ip);
	
	pthread_mutex_lock(&server->mutex_client_list);
	list_for_each_entry(rdma_trans, &server->client_list, client_entry)
	{
		inet_ntop(AF_INET, &(rdma_trans->peer_addr.sin_addr), travers_ip, sizeof(travers_ip));
		travers_ip_len=strlen(travers_ip);
		
		if(memcmp(cur_ip, travers_ip, max(cur_ip_len,travers_ip_len))==0)
		{
			INFO_LOG("find the same connection.");
			res_trans=rdma_trans;
			break;
		}
	}
	pthread_mutex_unlock(&server->mutex_client_list);

	return res_trans;
}

static int on_cm_connect_request(struct rdma_cm_event* event, 
										struct dhmp_transport* rdma_trans)
{
	struct dhmp_transport* new_trans,*normal_trans;
	struct rdma_conn_param conn_param;
	int i,retval=0;
	char* peer_addr;
	/*
	if(server->watcher_trans==NULL||server->cur_connections%2==0)
		normal_trans=NULL;
	else
		normal_trans=list_entry(server->client_list.prev, struct dhmp_transport, client_entry);
	*/
	normal_trans=dhmp_is_exist_connection(&event->id->route.addr.dst_sin);
	if(normal_trans)
		new_trans=dhmp_transport_create(rdma_trans->ctx, rdma_trans->device,
									false, true);
	else
		new_trans=dhmp_transport_create(rdma_trans->ctx, rdma_trans->device,
									false, false);
	if(!new_trans)
	{
		ERROR_LOG("rdma trans process connect request error.");
		return -1;
	}
	
	new_trans->link_trans=NULL;
	new_trans->cm_id=event->id;
	event->id->context=new_trans;
	
	retval=dhmp_qp_create(new_trans);
	if(retval)
	{
		ERROR_LOG("dhmp qp create error.");
		goto out;
	}

	if(server->watcher_trans==NULL)
		server->watcher_trans=new_trans;
	else
	{
		++server->cur_connections;
		pthread_mutex_lock(&server->mutex_client_list);
		list_add_tail(&new_trans->client_entry, &server->client_list);
		pthread_mutex_unlock(&server->mutex_client_list);
	}
	
	if(normal_trans)
	{
		normal_trans->link_trans=new_trans;
		new_trans->link_trans=normal_trans;
	}
	
	memset(&conn_param, 0, sizeof(conn_param));
	conn_param.retry_count=100;
	conn_param.rnr_retry_count=200;
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;
	
	retval=rdma_accept(new_trans->cm_id, &conn_param);
	if(retval)
	{
		ERROR_LOG("rdma accept error.");
		return -1;
	}
	
	new_trans->trans_state=DHMP_TRANSPORT_STATE_CONNECTING;
	dhmp_post_all_recv(new_trans);
	return retval;

out:
	free(new_trans);
	return retval;
}

static int on_cm_established(struct rdma_cm_event* event, struct dhmp_transport* rdma_trans)
{
	int retval=0;

	memcpy(&rdma_trans->local_addr,
			&rdma_trans->cm_id->route.addr.src_sin,
			sizeof(rdma_trans->local_addr));

	memcpy(&rdma_trans->peer_addr,
			&rdma_trans->cm_id->route.addr.dst_sin,
			sizeof(rdma_trans->peer_addr));
	
	rdma_trans->trans_state=DHMP_TRANSPORT_STATE_CONNECTED;
	return retval;
}

/**
 *	dhmp_destroy_source: destroy the used RDMA resouces
 */
static void dhmp_destroy_source(struct dhmp_transport* rdma_trans)
{
	if(rdma_trans->send_mr.addr)
	{
		ibv_dereg_mr(rdma_trans->send_mr.mr);
		free(rdma_trans->send_mr.addr);
	}

	if(rdma_trans->recv_mr.addr)
	{
		ibv_dereg_mr(rdma_trans->recv_mr.mr);
		free(rdma_trans->recv_mr.addr);
	}
	
	rdma_destroy_qp(rdma_trans->cm_id);
	dhmp_context_del_event_fd(rdma_trans->ctx, rdma_trans->dcq->comp_channel->fd);
	dhmp_context_del_event_fd(rdma_trans->ctx, rdma_trans->event_channel->fd);
}

static int on_cm_disconnected(struct rdma_cm_event* event, struct dhmp_transport* rdma_trans)
{
	dhmp_destroy_source(rdma_trans);
	rdma_trans->trans_state=DHMP_TRANSPORT_STATE_DISCONNECTED;
	if(server!=NULL&&rdma_trans!=server->watcher_trans)
	{
		--server->cur_connections;
		pthread_mutex_lock(&server->mutex_client_list);
		list_del(&rdma_trans->client_entry);
		pthread_mutex_unlock(&server->mutex_client_list);
		dhmp_inform_watcher_func(server->watcher_trans);
	}
	
	return 0;
}

static int on_cm_error(struct rdma_cm_event* event, struct dhmp_transport* rdma_trans)
{
	dhmp_destroy_source(rdma_trans);
	rdma_trans->trans_state=DHMP_TRANSPORT_STATE_ERROR;
	if(server!=NULL&&rdma_trans!=server->watcher_trans)
	{
		--server->cur_connections;
		pthread_mutex_lock(&server->mutex_client_list);
		list_del(&rdma_trans->client_entry);
		pthread_mutex_unlock(&server->mutex_client_list);
		dhmp_inform_watcher_func(server->watcher_trans);
	}
	return 0;
}

/*
 *	the function use for handling the event of event channel
 */
static int dhmp_handle_ec_event(struct rdma_cm_event* event)
{
	int retval=0;
	struct dhmp_transport* rdma_trans;
	
	rdma_trans=(struct dhmp_transport*) event->id->context;

	INFO_LOG("cm event [%s],status:%d",
	           rdma_event_str(event->event),event->status);

	switch(event->event)
	{
		case RDMA_CM_EVENT_ADDR_RESOLVED:
			retval=on_cm_addr_resolved(event, rdma_trans);
			break;
		case RDMA_CM_EVENT_ROUTE_RESOLVED:
			retval=on_cm_route_resolved(event, rdma_trans);
			break;
		case RDMA_CM_EVENT_CONNECT_REQUEST:
			retval=on_cm_connect_request(event,rdma_trans);
			break;
		case RDMA_CM_EVENT_ESTABLISHED:
			retval=on_cm_established(event,rdma_trans);
			break;
		case RDMA_CM_EVENT_DISCONNECTED:
			retval=on_cm_disconnected(event,rdma_trans);
			break;
		case RDMA_CM_EVENT_CONNECT_ERROR:
			retval=on_cm_error(event, rdma_trans);
			break;
		default:
			//ERROR_LOG("occur the other error.");
			retval=-1;
			break;
	};

	return retval;
}


static void dhmp_event_channel_handler(int fd, void* data)
{
	struct rdma_event_channel* ec=(struct rdma_event_channel*) data;
	struct rdma_cm_event* event,event_copy;
	int retval=0;

	event=NULL;
	while(( retval=rdma_get_cm_event(ec, &event) ==0))
	{
		memcpy(&event_copy, event, sizeof(*event));

		/*
		 * note: rdma_ack_cm_event function will clear event content
		 * so need to copy event content into event_copy.
		 */
		rdma_ack_cm_event(event);

		if(dhmp_handle_ec_event(&event_copy))
			break;
	}

	if(retval && errno!=EAGAIN)
	{
		ERROR_LOG("rdma get cm event error.");
	}
}

static int dhmp_event_channel_create(struct dhmp_transport* rdma_trans)
{
	int flags,retval=0;

	rdma_trans->event_channel=rdma_create_event_channel();
	if(!rdma_trans->event_channel)
	{
		ERROR_LOG("rdma create event channel error.");
		return -1;
	}

	flags=fcntl(rdma_trans->event_channel->fd, F_GETFL, 0);
	if(flags!=-1)
		flags=fcntl(rdma_trans->event_channel->fd,
		              F_SETFL, flags|O_NONBLOCK);

	if(flags==-1)
	{
		retval=-1;
		ERROR_LOG("set event channel nonblock error.");
		goto clean_ec;
	}

	dhmp_context_add_event_fd(rdma_trans->ctx,
								EPOLLIN,
	                            rdma_trans->event_channel->fd,
	                            rdma_trans->event_channel,
	                            dhmp_event_channel_handler);
	return retval;

clean_ec:
	rdma_destroy_event_channel(rdma_trans->event_channel);
	return retval;
}

static int dhmp_memory_register(struct ibv_pd *pd, 
									struct dhmp_mr *dmr, size_t length)
{
	dmr->addr=malloc(length);
	if(!dmr->addr)
	{
		ERROR_LOG("allocate mr memory error.");
		return -1;
	}

	dmr->mr=ibv_reg_mr(pd, dmr->addr, length, IBV_ACCESS_LOCAL_WRITE);
	if(!dmr->mr)
	{
		ERROR_LOG("rdma register memory error.");
		goto out;
	}

	dmr->cur_pos=0;
	return 0;

out:
	free(dmr->addr);
	return -1;
}

struct dhmp_transport* dhmp_transport_create(struct dhmp_context* ctx, 
													struct dhmp_device* dev,
													bool is_listen,
													bool is_poll_qp)
{
	struct dhmp_transport *rdma_trans;
	int err=0;
	
	rdma_trans=(struct dhmp_transport*)malloc(sizeof(struct dhmp_transport));
	if(!rdma_trans)
	{
		ERROR_LOG("allocate memory error");
		return NULL;
	}

	rdma_trans->trans_state=DHMP_TRANSPORT_STATE_INIT;
	rdma_trans->ctx=ctx;
	rdma_trans->device=dev;
	rdma_trans->dram_used_size=rdma_trans->nvm_used_size=0;
	
	err=dhmp_event_channel_create(rdma_trans);
	if(err)
	{
		ERROR_LOG("dhmp event channel create error");
		goto out;
	}

	if(!is_listen)
	{
		err=dhmp_memory_register(rdma_trans->device->pd,
								&rdma_trans->send_mr,
								SEND_REGION_SIZE);
		if(err)
			goto out_event_channel;

		err=dhmp_memory_register(rdma_trans->device->pd,
								&rdma_trans->recv_mr,
								RECV_REGION_SIZE);
		if(err)
			goto out_send_mr;
		
		rdma_trans->is_poll_qp=is_poll_qp;
	}
	
	return rdma_trans;
out_send_mr:
	ibv_dereg_mr(rdma_trans->send_mr.mr);
	free(rdma_trans->send_mr.addr);
	
out_event_channel:
	dhmp_context_del_event_fd(rdma_trans->ctx, rdma_trans->event_channel->fd);
	rdma_destroy_event_channel(rdma_trans->event_channel);
	
out:
	free(rdma_trans);
	return NULL;
}

int dhmp_transport_listen(struct dhmp_transport* rdma_trans, int listen_port)
{
	int retval=0, backlog;
	struct sockaddr_in addr;

	retval=rdma_create_id(rdma_trans->event_channel,
	                        &rdma_trans->cm_id,
	                        rdma_trans, RDMA_PS_TCP);
	if(retval)
	{
		ERROR_LOG("rdma create id error.");
		return retval;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_port=htons(listen_port);

	retval=rdma_bind_addr(rdma_trans->cm_id,
	                       (struct sockaddr*) &addr);
	if(retval)
	{
		ERROR_LOG("rdma bind addr error.");
		goto cleanid;
	}

	backlog=10;
	retval=rdma_listen(rdma_trans->cm_id, backlog);
	if(retval)
	{
		ERROR_LOG("rdma listen error.");
		goto cleanid;
	}

	rdma_trans->trans_state=DHMP_TRANSPORT_STATE_LISTEN;
	INFO_LOG("rdma listening on port %d",
	           ntohs(rdma_get_src_port(rdma_trans->cm_id)));

	return retval;

cleanid:
	rdma_destroy_id(rdma_trans->cm_id);
	rdma_trans->cm_id=NULL;

	return retval;
}

static int dhmp_port_uri_transfer(struct dhmp_transport* rdma_trans,
										const char* url, int port)
{
	struct sockaddr_in peer_addr;
	int retval=0;

	memset(&peer_addr,0,sizeof(peer_addr));
	peer_addr.sin_family=AF_INET;
	peer_addr.sin_port=htons(port);

	retval=inet_pton(AF_INET, url, &peer_addr.sin_addr);
	if(retval<=0)
	{
		ERROR_LOG("IP Transfer Error.");
		goto out;
	}

	memcpy(&rdma_trans->peer_addr, &peer_addr, sizeof(struct sockaddr_in));

out:
	return retval;
}

int dhmp_transport_connect(struct dhmp_transport* rdma_trans,
                             const char* url, int port)
{
	int retval=0;
	if(!url||port<=0)
	{
		ERROR_LOG("url or port input error.");
		return -1;
	}

	retval=dhmp_port_uri_transfer(rdma_trans, url, port);
	if(retval<0)
	{
		ERROR_LOG("rdma init port uri error.");
		return retval;
	}

	/*rdma_cm_id dont init the rdma_cm_id's verbs*/
	retval=rdma_create_id(rdma_trans->event_channel,
						&rdma_trans->cm_id,
						rdma_trans, RDMA_PS_TCP);
	if(retval)
	{
		ERROR_LOG("rdma create id error.");
		goto clean_rdmatrans;
	}
	retval=rdma_resolve_addr(rdma_trans->cm_id, NULL,
	                          (struct sockaddr*) &rdma_trans->peer_addr,
	                           ADDR_RESOLVE_TIMEOUT);
	if(retval)
	{
		ERROR_LOG("RDMA Device resolve addr error.");
		goto clean_cmid;
	}
	
	rdma_trans->trans_state=DHMP_TRANSPORT_STATE_CONNECTING;
	return retval;

clean_cmid:
	rdma_destroy_id(rdma_trans->cm_id);

clean_rdmatrans:
	rdma_trans->cm_id=NULL;

	return retval;
}

/*
 *	two sided RDMA operations
 */
static void dhmp_post_recv(struct dhmp_transport* rdma_trans, void *addr)
{
	struct ibv_recv_wr recv_wr, *bad_wr_ptr=NULL;
	struct ibv_sge sge;
	struct dhmp_task *recv_task_ptr;
	int err=0;

	if(rdma_trans->trans_state>DHMP_TRANSPORT_STATE_CONNECTED)
		return ;
	
	recv_task_ptr=dhmp_recv_task_create(rdma_trans, addr);
	if(!recv_task_ptr)
	{
		ERROR_LOG("create recv task error.");
		return ;
	}
	
	recv_wr.wr_id=(uintptr_t)recv_task_ptr;
	recv_wr.next=NULL;
	recv_wr.sg_list=&sge;
	recv_wr.num_sge=1;

	sge.addr=(uintptr_t)recv_task_ptr->sge.addr;
	sge.length=recv_task_ptr->sge.length;
	sge.lkey=recv_task_ptr->sge.lkey;
	
	err=ibv_post_recv(rdma_trans->qp, &recv_wr, &bad_wr_ptr);
	if(err)
		ERROR_LOG("ibv post recv error.");
	
}

/**
 *	dhmp_post_all_recv:loop call the dhmp_post_recv function
 */
static void dhmp_post_all_recv(struct dhmp_transport *rdma_trans)
{
	int i, single_region_size=0;

	if(rdma_trans->is_poll_qp)
		single_region_size=SINGLE_POLL_RECV_REGION;
	else
		single_region_size=SINGLE_NORM_RECV_REGION;
	
	for(i=0; i<RECV_REGION_SIZE/single_region_size; i++)
	{
		dhmp_post_recv(rdma_trans, 
			rdma_trans->recv_mr.addr+i*single_region_size);
	}
}

void dhmp_post_send(struct dhmp_transport* rdma_trans, struct dhmp_msg* msg_ptr)
{
	struct ibv_send_wr send_wr,*bad_wr=NULL;
	struct ibv_sge sge;
	struct dhmp_task *send_task_ptr;
	int err=0;
	
	if(rdma_trans->trans_state!=DHMP_TRANSPORT_STATE_CONNECTED)
		return ;
	send_task_ptr=dhmp_send_task_create(rdma_trans, msg_ptr);
	if(!send_task_ptr)
	{
		ERROR_LOG("create recv task error.");
		return ;
	}
	
	memset ( &send_wr, 0, sizeof ( send_wr ) );
	send_wr.wr_id= ( uintptr_t ) send_task_ptr;
	send_wr.sg_list=&sge;
	send_wr.num_sge=1;
	send_wr.opcode=IBV_WR_SEND;
	send_wr.send_flags=IBV_SEND_SIGNALED;

	sge.addr= ( uintptr_t ) send_task_ptr->sge.addr;
	sge.length=send_task_ptr->sge.length;
	sge.lkey=send_task_ptr->sge.lkey;

	err=ibv_post_send ( rdma_trans->qp, &send_wr, &bad_wr );
	if ( err )
		ERROR_LOG ( "ibv_post_send error." );

}

static struct dhmp_send_mr* dhmp_get_mr_from_send_list(struct dhmp_transport* rdma_trans, void* addr, int length )
{
	struct dhmp_send_mr *res,*tmp;
	void* new_addr=NULL;

	res=(struct dhmp_send_mr* )malloc(sizeof(struct dhmp_send_mr));
	if(!res)
	{
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	
#ifdef DHMP_MR_REUSE_POLICY
	if(length>RDMA_SEND_THREASHOLD)
	{
#endif

		res->mr=ibv_reg_mr(rdma_trans->device->pd,
							addr, length, IBV_ACCESS_LOCAL_WRITE);
		if(!res->mr)
		{
			ERROR_LOG("ibv register memory error.");
			goto error;
		}
		
#ifdef DHMP_MR_REUSE_POLICY
	}
	else
	{
		pthread_mutex_lock(&client->mutex_send_mr_list);
		list_for_each_entry(tmp, &client->send_mr_list, send_mr_entry)
		{
			if(tmp->mr->length >= length)
				break;
		}
		
		if((&tmp->send_mr_entry) == (&client->send_mr_list))
		{
			pthread_mutex_unlock(&client->mutex_send_mr_list);
			new_addr=malloc(length);
			if(!new_addr)
			{
				ERROR_LOG("allocate memory error.");
				goto error;
			}

			res->mr=ibv_reg_mr ( rdma_trans->device->pd, 
								new_addr, length, IBV_ACCESS_LOCAL_WRITE );
			if(!res->mr)
			{
				ERROR_LOG("ibv reg memory error.");
				free(new_addr);
				goto error;
			}
		}
		else
		{
			free(res);
			res=tmp;
			list_del(&res->send_mr_entry);
			pthread_mutex_unlock(&client->mutex_send_mr_list);
		}
	}
#endif

	return res;

error:
	free ( res );
	return NULL;
}

int dhmp_rdma_read(struct dhmp_transport* rdma_trans, struct ibv_mr* mr, void* local_addr, int length)
{
	struct dhmp_task* read_task;
	struct ibv_send_wr send_wr,*bad_wr=NULL;
	struct ibv_sge sge;
	struct dhmp_send_mr* smr=NULL;
	int err=0;
	
	smr=dhmp_get_mr_from_send_list(rdma_trans, local_addr, length);
	read_task=dhmp_read_task_create(rdma_trans, smr, length);
	if ( !read_task )
	{
		ERROR_LOG ( "allocate memory error." );
		return -1;
	}

	memset(&send_wr, 0, sizeof(struct ibv_send_wr));

	send_wr.wr_id= ( uintptr_t ) read_task;
	send_wr.opcode=IBV_WR_RDMA_READ;
	send_wr.sg_list=&sge;
	send_wr.num_sge=1;
	send_wr.send_flags=IBV_SEND_SIGNALED;
	send_wr.wr.rdma.remote_addr=(uintptr_t)mr->addr;
	send_wr.wr.rdma.rkey=mr->rkey;

	sge.addr=(uintptr_t)read_task->sge.addr;
	sge.length=read_task->sge.length;
	sge.lkey=read_task->sge.lkey;
	
	err=ibv_post_send(rdma_trans->qp, &send_wr, &bad_wr);
	if(err)
	{
		ERROR_LOG("ibv_post_send error");
		goto error;
	}

	DEBUG_LOG("before local addr is %s", local_addr);
	
	while(!read_task->done_flag);
	
#ifdef DHMP_MR_REUSE_POLICY
	if(length > RDMA_SEND_THREASHOLD)
	{
#endif

		ibv_dereg_mr(smr->mr);
		free(smr);
		
#ifdef DHMP_MR_REUSE_POLICY
	}
	else
	{
		memcpy(local_addr, read_task->sge.addr, length);
		pthread_mutex_lock(&client->mutex_send_mr_list);
		list_add(&smr->send_mr_entry, &client->send_mr_list);
		pthread_mutex_unlock(&client->mutex_send_mr_list);
	}
#endif

	DEBUG_LOG("local addr content is %s", local_addr);

	return 0;
error:
	return -1;
}

int dhmp_rdma_write ( struct dhmp_transport* rdma_trans, struct dhmp_addr_info *addr_info, struct ibv_mr* mr, void* local_addr, int length, int dram_flag )
{
	struct dhmp_task* write_task;
	struct ibv_send_wr send_wr,*bad_wr=NULL;
	struct ibv_sge sge;
	struct dhmp_send_mr* smr=NULL;
	int err=0;
	
	smr=dhmp_get_mr_from_send_list(rdma_trans, local_addr, length);
	write_task=dhmp_write_task_create(rdma_trans, smr, length);
	if(!write_task)
	{
		ERROR_LOG("allocate memory error.");
		return -1;
	}
	write_task->addr_info=addr_info;
	
	memset(&send_wr, 0, sizeof(struct ibv_send_wr));

	send_wr.wr_id= ( uintptr_t ) write_task;
	send_wr.opcode=IBV_WR_RDMA_WRITE;
	send_wr.sg_list=&sge;
	send_wr.num_sge=1;
	send_wr.send_flags=IBV_SEND_SIGNALED;
	send_wr.wr.rdma.remote_addr= ( uintptr_t ) mr->addr;
	send_wr.wr.rdma.rkey=mr->rkey;

	sge.addr= ( uintptr_t ) write_task->sge.addr;
	sge.length=write_task->sge.length;
	sge.lkey=write_task->sge.lkey;

#ifdef DHMP_MR_REUSE_POLICY
	if(length<=RDMA_SEND_THREASHOLD)
		memcpy(write_task->sge.addr, local_addr, length);
#endif

	err=ibv_post_send ( rdma_trans->qp, &send_wr, &bad_wr );
	if ( err )
	{
		ERROR_LOG("ibv_post_send error");
		exit(-1);
		goto error;
	}
	
	//while(!write_task->done_flag);

#ifdef DHMP_MR_REUSE_POLICY
	if(length>RDMA_SEND_THREASHOLD)
	{
#endif
		while(!write_task->done_flag);
		ibv_dereg_mr(smr->mr);
		free(smr);

#ifdef DHMP_MR_REUSE_POLICY
	}
#endif
	
	return 0;
	
error:
	return -1;
}

