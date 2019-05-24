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

struct dhmp_watcher *watcher=NULL;

struct dhmp_device *dhmp_get_dev_from_watcher()
{
	struct dhmp_device *res_dev_ptr=NULL;
	if(!list_empty(&watcher->dev_list))
	{
		res_dev_ptr=list_first_entry(&watcher->dev_list,
									struct dhmp_device,
									dev_entry);
	}
		
	return res_dev_ptr;
}

json_object *dhmp_get_json_data()
{
	int i,j;
	char server_name[25], app_name[25];
	
	json_object *dram_total_size_arr[DHMP_SERVER_NODE_NUM],
				*nvm_total_size_arr[DHMP_SERVER_NODE_NUM],
				*server_jobj, *app_jobj, *res,
				*dram_used_size,*nvm_used_size,
				*apps_arr;
	
	
	for(i=0; i<watcher->config.nets_cnt; i++)
	{
		dram_total_size_arr[i]=json_object_new_int64(watcher->servers_info[i].dram_total_size);
		nvm_total_size_arr[i]=json_object_new_int64(watcher->servers_info[i].nvm_total_size);
	}

	res=json_object_new_object();
	
	for(i=0;i <watcher->config.nets_cnt; i++)
	{
		snprintf(server_name, 24, "server%d",i+1);

		server_jobj=json_object_new_object();
    	json_object_object_add(server_jobj, "dram total size", dram_total_size_arr[i]);
    	json_object_object_add(server_jobj, "nvm total size", nvm_total_size_arr[i]);

		apps_arr=json_object_new_array();
		for(j=0; j<watcher->cur_app_num; j++)
		{
			app_jobj=json_object_new_object();
			
			snprintf(app_name, 24, "app%d", j+1);
			dram_used_size=json_object_new_int64(watcher->apps_info[i][j].dram_used_size);
			nvm_used_size=json_object_new_int64(watcher->apps_info[i][j].nvm_used_size);

			json_object_object_add(app_jobj, "app name", json_object_new_string(app_name));
			json_object_object_add(app_jobj, "dram used size", dram_used_size);
    		json_object_object_add(app_jobj, "nvm used size", nvm_used_size);
			
			
			json_object_array_add(apps_arr, app_jobj);
		}
		json_object_object_add(server_jobj, "apps", apps_arr);
		json_object_object_add(res, server_name, server_jobj);
	}

	return res;
}

void dhmp_inform_tcp_server()
{
	json_object *jobj;
	const char *jobj_str;
	int err, size;
	
	jobj=dhmp_get_json_data();
	jobj_str=json_object_to_json_string(jobj);
	size=strlen(jobj_str);

	memcpy(watcher->send_buff, jobj_str, strlen(jobj_str));
	watcher->send_buff[size]='\n';
	watcher->send_buff[size+1]='\0';
	
	err=write(watcher->tcp_sockfd, watcher->send_buff, BUFSIZ-1);
	if(err<0)
	{
		ERROR_LOG("connection close or exist error.");
		return ;
	}
	INFO_LOG("write success.%s",watcher->send_buff);

	err=read(watcher->tcp_sockfd, watcher->recv_buff, BUFSIZ-1);
	if(err<=0)
	{
		ERROR_LOG("connection close or exist error.");
		return ;
	}
	INFO_LOG("read content is %s", watcher->recv_buff);
	
}

int dhmp_build_tcp_connection()
{
	struct sockaddr_in server_addr;
	int err;
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family=AF_INET;
	server_addr.sin_addr.s_addr=inet_addr(watcher->config.watcher_addr);
	server_addr.sin_port=htons(watcher->config.watcher_port);

	watcher->tcp_sockfd=socket(AF_INET, SOCK_STREAM, 0);
	if(watcher->tcp_sockfd<0)
	{
		ERROR_LOG("socket call error.");
		return -1;
	}

	err=connect(watcher->tcp_sockfd, (struct sockaddr*)&server_addr,
				sizeof(server_addr));
	if(err)
	{
		ERROR_LOG("connect call error.");
		return -1;
	}
	INFO_LOG("tcp connection success.");
	return 0;
}

void *dhmp_inform_server_thread(void *data)
{
	uint64_t exp=0;
	
	while(1)
	{
		read(watcher->inform_server_fd, &exp, sizeof(uint64_t));
		dhmp_inform_tcp_server();
	}
	return NULL;
}

void dhmp_fetch_server_info(int node_index, struct dhmp_transport *rdma_trans)
{
	struct dhmp_msg msg;
	
	msg.msg_type=DHMP_MSG_SERVER_INFO_REQUEST;
	msg.data_size=sizeof(int);
	msg.data=&node_index;

	watcher->servers_info[node_index].nvm_total_size=0;
	
	dhmp_post_send(rdma_trans, &msg);

	while(watcher->servers_info[node_index].nvm_total_size<=0);

	INFO_LOG("node index %d",node_index);
	INFO_LOG("dram %ld",watcher->servers_info[node_index].dram_total_size);
	INFO_LOG("nvm %ld",watcher->servers_info[node_index].nvm_total_size);
}

void dhmp_watcher_init()
{
	int i;
	struct itimerspec watch_its;
	
	watcher=(struct dhmp_watcher *)malloc(sizeof(struct dhmp_watcher));
	if(!watcher)
	{
		ERROR_LOG("allocate memory error.");
		return ;
	}

	dhmp_hash_init();
	dhmp_config_init(&watcher->config, true);
	dhmp_context_init(&watcher->ctx);

	/*init list about rdma device*/
	INIT_LIST_HEAD(&watcher->dev_list);
	dhmp_dev_list_init(&watcher->dev_list);
	
	/*init normal connection*/
	memset(watcher->connect_trans, 0, DHMP_SERVER_NODE_NUM*
										sizeof(struct dhmp_transport*));
	for(i=0;i<watcher->config.nets_cnt;i++)
	{
		INFO_LOG("create the [%d]-th normal transport.",i);
		watcher->connect_trans[i]=dhmp_transport_create(&watcher->ctx, 
														dhmp_get_dev_from_watcher(),
														false,
														false);
		if(!watcher->connect_trans[i])
		{
			ERROR_LOG("create the [%d]-th transport error.",i);
			continue;
		}
		watcher->connect_trans[i]->node_id=i;
		dhmp_transport_connect(watcher->connect_trans[i],
							watcher->config.net_infos[i].addr,
							watcher->config.net_infos[i].port);
	}

	for(i=0;i<watcher->config.nets_cnt;i++)
	{
		if(watcher->connect_trans[i]==NULL)
			continue;
		while(watcher->connect_trans[i]->trans_state<DHMP_TRANSPORT_STATE_CONNECTED);
	}

	for(i=0; i<watcher->config.nets_cnt; i++)
		dhmp_fetch_server_info(i, watcher->connect_trans[i]);
	
	dhmp_build_tcp_connection();

	watch_its.it_value.tv_sec = 2;
	watch_its.it_value.tv_nsec = 0;
	watch_its.it_interval.tv_sec = 2;
	watch_its.it_interval.tv_nsec = 0;
	watcher->inform_server_fd=dhmp_timerfd_create(&watch_its);
	pthread_create(&watcher->inform_server_thread, NULL, dhmp_inform_server_thread, (void*)watcher);
	
}

void dhmp_watcher_destroy()
{
	INFO_LOG("warcher destroy start.");
	pthread_join(watcher->ctx.epoll_thread, NULL);
	INFO_LOG("warcher destroy end.");
	
	free(watcher);
}

