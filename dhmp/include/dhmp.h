#ifndef DHMP_H
#define DHMP_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/time.h>
#include <linux/list.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <numa.h>
#include "json-c/json.h"

#define DHMP_CACHE_POLICY
#define DHMP_MR_REUSE_POLICY

#define DHMP_SERVER_DRAM_TH (1024*1024*1024)
#define DHMP_SERVER_NODE_NUM 3

#define DHMP_DEFAULT_SIZE 256
#define DHMP_DEFAULT_POLL_TIME 200000000

#define DHMP_MAX_OBJ_NUM 20000
#define DHMP_MAX_CLIENT_NUM 100

#define PAGE_SIZE 4096
#define NANOSECOND (1000000000)

#define DHMP_RTT_TIME (6000)
#define DHMP_DRAM_RW_TIME (260)

#define max(a,b) (a>b?a:b)
#define min(a,b) (a>b?b:a)

#ifndef bool
#define bool char
#define true 1
#define false 0
#endif

enum dhmp_msg_type{
	DHMP_MSG_MALLOC_REQUEST,
	DHMP_MSG_MALLOC_RESPONSE,
	DHMP_MSG_MALLOC_ERROR,
	DHMP_MSG_FREE_REQUEST,
	DHMP_MSG_FREE_RESPONSE,
	DHMP_MSG_APPLY_DRAM_REQUEST,
	DHMP_MSG_APPLY_DRAM_RESPONSE,
	DHMP_MSG_CLEAR_DRAM_REQUEST,
	DHMP_MSG_CLEAR_DRAM_RESPONSE,
	DHMP_MSG_MEM_CHANGE,
	DHMP_MSG_SERVER_INFO_REQUEST,
	DHMP_MSG_SERVER_INFO_RESPONSE,
	DHMP_MSG_CLOSE_CONNECTION
};

/*struct dhmp_msg:use for passing control message*/
struct dhmp_msg{
	enum dhmp_msg_type msg_type;
	size_t data_size;
	void *data;
};

/*struct dhmp_addr_info is the addr struct in cluster*/
struct dhmp_addr_info{
	int read_cnt;
	int write_cnt;
	int node_index;
	bool write_flag;
	struct ibv_mr dram_mr;
	struct ibv_mr nvm_mr;
	struct hlist_node addr_entry;
};

/*dhmp malloc request msg*/
struct dhmp_mc_request{
	size_t req_size;
	struct dhmp_addr_info *addr_info;
};

/*dhmp malloc response msg*/
struct dhmp_mc_response{
	struct dhmp_mc_request req_info;
	struct ibv_mr mr;
};

/*dhmp free memory request msg*/
struct dhmp_free_request{
	struct dhmp_addr_info *addr_info;
	struct ibv_mr mr;
};

/*dhmp free memory response msg*/
struct dhmp_free_response{
	struct dhmp_addr_info *addr_info;
};

struct dhmp_dram_info{
	void *nvm_addr;
	struct ibv_mr dram_mr;
};

/**
 *	dhmp_malloc: remote alloc the size of length region
 */
void *dhmp_malloc(size_t length);

/**
 *	dhmp_read:read the data from dhmp_addr, write into the local_buf
 */
int dhmp_read(void *dhmp_addr, void * local_buf, size_t count);

/**
 *	dhmp_write:write the data in local buf into the dhmp_addr position
 */
int dhmp_write(void *dhmp_addr, void * local_buf, size_t count);

/**
 *	dhmp_free:release remote memory
 */
void dhmp_free(void *dhmp_addr);

/**
 *	dhmp_client_init:init the dhmp client
 *	note:client connect the all server
 */
void dhmp_client_init();

/**
 *	dhmp_client_destroy:clean RDMA resources
 */
void dhmp_client_destroy();

/**
 *	dhmp_server_init:init server 
 *	include config.xml read, rdma listen,
 *	register memory, context run. 
 */
void dhmp_server_init();

/**
 *	dhmp_server_destroy: close the context,
 *	clean the rdma resource
 */
void dhmp_server_destroy();

#endif
