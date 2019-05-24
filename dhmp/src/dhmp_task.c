#include "dhmp.h"
#include "dhmp_hash.h"
#include "dhmp_log.h"
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

struct dhmp_task* dhmp_recv_task_create(struct dhmp_transport* rdma_trans, 
										void *addr )
{
	struct dhmp_task *recv_task;

	recv_task=malloc(sizeof(struct dhmp_task));
	if(!recv_task)
	{
		ERROR_LOG("allocate memory error.");
		return NULL;
	}

	/*init the recv task*/
	recv_task->sge.addr=addr;
	
	/*according to the flag of is_poll_qp,
	 decide to post what size sge*/
	if(rdma_trans->is_poll_qp)
		recv_task->sge.length=SINGLE_POLL_RECV_REGION;
	else
		recv_task->sge.length=SINGLE_NORM_RECV_REGION;
	
	recv_task->sge.lkey=rdma_trans->recv_mr.mr->lkey;
	recv_task->rdma_trans=rdma_trans;

	return recv_task;
}

struct dhmp_task* dhmp_send_task_create(struct dhmp_transport* rdma_trans,
										struct dhmp_msg *msg)
{
	struct dhmp_task *send_task;
	struct dhmp_mr *send_mr=&rdma_trans->send_mr;
	
	send_task=malloc(sizeof(struct dhmp_task));
	if(!send_task)
	{
		ERROR_LOG("allocate memory error.");
		return NULL;
	}

	send_task->sge.length=sizeof(enum dhmp_msg_type)+
							sizeof(size_t)+
							msg->data_size;
	
	if(send_mr->cur_pos+send_task->sge.length>SEND_REGION_SIZE)
		send_mr->cur_pos=0;
	
	send_task->sge.addr=send_mr->addr+send_mr->cur_pos;
	send_task->sge.lkey=send_mr->mr->lkey;
	send_task->rdma_trans=rdma_trans;
	
	send_mr->cur_pos+=send_task->sge.length;
	
	/*use msg build send task*/
	memcpy(send_task->sge.addr, &msg->msg_type, sizeof(enum dhmp_msg_type));
	memcpy(send_task->sge.addr+sizeof(enum dhmp_msg_type), 
			&msg->data_size, sizeof(size_t));
	memcpy(send_task->sge.addr+sizeof(enum dhmp_msg_type)+sizeof(size_t), 
			msg->data, msg->data_size);
	
	return send_task;

}

struct dhmp_task* dhmp_read_task_create(struct dhmp_transport* rdma_trans,
										struct dhmp_send_mr *smr,
										int length)
{
	struct dhmp_task *read_task;
	
	read_task=malloc(sizeof(struct dhmp_task));
	if(!read_task)
	{
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	
	read_task->done_flag=false;
	read_task->rdma_trans=rdma_trans;
	read_task->sge.addr=smr->mr->addr;
	read_task->sge.length=length;
	read_task->sge.lkey=smr->mr->lkey;
	read_task->smr=smr;

	return read_task;
}


struct dhmp_task* dhmp_write_task_create(struct dhmp_transport* rdma_trans,
										struct dhmp_send_mr *smr,
										int length)
{
	struct dhmp_task *write_task;
	
	write_task=malloc(sizeof(struct dhmp_task));
	if(!write_task)
	{
		ERROR_LOG("allocate memory error.");
		return NULL;
	}
	
	write_task->done_flag=false;
	write_task->rdma_trans=rdma_trans;
	write_task->sge.addr=smr->mr->addr;
	write_task->sge.length=length;
	write_task->sge.lkey=smr->mr->lkey;
	write_task->smr=smr;

	return write_task;
}