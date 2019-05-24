#ifndef DHMP_SERVER_H
#define DHMP_SERVER_H

/*decide the buddy system's order*/
#define MAX_ORDER 5
#define SINGLE_AREA_SIZE 2097152

#define DHMP_DRAM_HT_SIZE 251

extern const size_t buddy_size[MAX_ORDER];

struct dhmp_free_block{
	void *addr;
	size_t size;
	struct ibv_mr *mr;
	struct list_head free_block_entry;
};

struct dhmp_free_block_head{
	struct list_head free_block_list;
	int nr_free;
};

struct dhmp_area{
	/*be able to alloc max size=buddy_size[max_index]*/
	int max_index;
	//pthread_spinlock_t mutex_area;
	struct ibv_mr *mr;
	struct list_head area_entry;
	struct dhmp_free_block_head block_head[MAX_ORDER];
};

struct dhmp_dram_entry{
	void *nvm_addr;
	size_t	length;
	struct ibv_mr *dram_mr;
	struct hlist_node dram_node;
};

struct dhmp_server{
	struct dhmp_context ctx;
	struct dhmp_config config;

	struct dhmp_transport *listen_trans;
	struct dhmp_transport *watcher_trans;
	
	struct list_head dev_list;
	pthread_mutex_t mutex_client_list;
	struct list_head client_list;

	/*below structure about area*/
	struct dhmp_area *cur_area;
	struct list_head area_list;
	struct list_head more_area_list;

	int cur_connections;
	long dram_used_size;
	long nvm_used_size;
	long dram_total_size;
	long nvm_total_size;

	struct hlist_head dram_ht[DHMP_DRAM_HT_SIZE];
};

extern struct dhmp_server *server;

struct dhmp_area *dhmp_area_create(bool is_init_buddy, size_t length);
int dhmp_hash_in_server(void *nvm_addr);
struct dhmp_device *dhmp_get_dev_from_server();


#endif


