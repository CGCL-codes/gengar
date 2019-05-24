#ifndef DHMP_TASK_H
#define DHMP_TASK_H

struct dhmp_sge
{
	void *addr;
	size_t length;
	uint32_t lkey;
};

struct dhmp_task
{
	bool done_flag;
	struct dhmp_sge sge;
	struct dhmp_transport* rdma_trans;
	struct dhmp_send_mr *smr;
	struct dhmp_addr_info *addr_info;
};

struct dhmp_task* dhmp_recv_task_create(struct dhmp_transport* rdma_trans,
										void *addr);

struct dhmp_task* dhmp_send_task_create(struct dhmp_transport* rdma_trans,
										struct dhmp_msg *msg);

struct dhmp_task* dhmp_read_task_create(struct dhmp_transport* rdma_trans,
										struct dhmp_send_mr *send_mr,
										int length);

struct dhmp_task* dhmp_write_task_create(struct dhmp_transport* rdma_trans,
										struct dhmp_send_mr *smr,
										int length);
#endif


