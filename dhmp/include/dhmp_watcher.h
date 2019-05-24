#ifndef DHMP_WATCHER_H
#define DHMP_WATCHER_H

struct dhmp_app_mem_info{
	long dram_used_size;
	long nvm_used_size;
};

struct dhmp_server_mem_info{
	long dram_total_size;
	long nvm_total_size;
};

struct dhmp_watcher{
	struct dhmp_context ctx;
	struct dhmp_config config;

	struct list_head dev_list;

	struct dhmp_transport *connect_trans[DHMP_SERVER_NODE_NUM];
	
	struct dhmp_server_mem_info servers_info[DHMP_SERVER_NODE_NUM];
	struct dhmp_app_mem_info apps_info[DHMP_SERVER_NODE_NUM][DHMP_MAX_CLIENT_NUM];
	int cur_app_num;
	
	int tcp_sockfd;

	char send_buff[BUFSIZ];
	char recv_buff[BUFSIZ];

	int inform_server_fd;
	pthread_t inform_server_thread;
};

extern struct dhmp_watcher *watcher;

void dhmp_inform_tcp_server();

#endif



