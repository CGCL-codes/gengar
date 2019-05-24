#ifndef DHMP_WORK_H
#define DHMP_WORK_H

struct dhmp_rw_work{
	struct dhmp_transport *rdma_trans;
	void *dhmp_addr;
	void *local_addr;
	struct dhmp_send_mr *smr;
	size_t length;
	bool done_flag;
};

struct dhmp_malloc_work{
	struct dhmp_transport *rdma_trans;
	struct dhmp_addr_info *addr_info;
	void *res_addr;
	size_t length;
	bool done_flag;
};

struct dhmp_free_work{
	struct dhmp_transport *rdma_trans;
	void *dhmp_addr;
	bool done_flag;
};

struct dhmp_close_work{
	struct dhmp_transport *rdma_trans;
	bool done_flag;
};

enum dhmp_work_type{
	DHMP_WORK_MALLOC,
	DHMP_WORK_FREE,
	DHMP_WORK_READ,
	DHMP_WORK_WRITE,
	DHMP_WORK_POLL,
	DHMP_WORK_CLOSE,
	DHMP_WORK_DONE
};

struct dhmp_work{
	enum dhmp_work_type work_type;
	void *work_data;
	struct list_head work_entry;
};

void *dhmp_work_handle_thread(void *data);
int dhmp_hash_in_client(void *addr);
void *dhmp_transfer_dhmp_addr(struct dhmp_transport *rdma_trans,
									void *normal_addr);
struct dhmp_addr_info *dhmp_get_addr_info_from_ht(int index, void *dhmp_addr);

#endif



