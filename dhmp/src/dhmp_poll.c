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

struct dhmp_nvm_info hot_nvm[DHMP_SERVER_NODE_NUM][DHMP_MAX_OBJ_NUM],
					cold_nvm[DHMP_SERVER_NODE_NUM][DHMP_MAX_OBJ_NUM];
int hot_cnt[DHMP_SERVER_NODE_NUM], cold_cnt[DHMP_SERVER_NODE_NUM];
struct dhmp_sort_addr_entry sa_entries[DHMP_SERVER_NODE_NUM][DHMP_MAX_OBJ_NUM];
int max_cnt_test=0;

/**
 * use for compare function.
 */
int comp_sort_addr_entry(const void*a,const void*b)
{	
    struct dhmp_sort_addr_entry *sae_a, *sae_b;
	sae_a=(struct dhmp_sort_addr_entry*)a;
	sae_b=(struct dhmp_sort_addr_entry*)b;

    return sae_a->rwcnt-sae_b->rwcnt;
}

void dhmp_handle_cache_model(int node_index, int length)
{
	int start,end;
	start=0;
	end=length-1;

	cold_cnt[node_index]=hot_cnt[node_index]=0;
	client->req_dram_size[node_index]=0;
	
	while(start<end)
	{
		while(start<end&&sa_entries[node_index][start].in_dram==false)
			++start;
		
		while(end>start&&sa_entries[node_index][end].in_dram==true)
			--end;
		
		if(start<end)
		{
			cold_nvm[node_index][cold_cnt[node_index]].nvm_addr=sa_entries[node_index][start].nvm_addr;
			cold_nvm[node_index][cold_cnt[node_index]].length=sa_entries[node_index][start].length;
			++cold_cnt[node_index];

			hot_nvm[node_index][hot_cnt[node_index]].nvm_addr=sa_entries[node_index][end].nvm_addr;
			hot_nvm[node_index][hot_cnt[node_index]].length=sa_entries[node_index][end].length;
			client->req_dram_size[node_index]+=hot_nvm[node_index][hot_cnt[node_index]].length;
			++hot_cnt[node_index];

			++start;
			--end;
		}
	}
	
	--start;
	while(start>=0&&sa_entries[node_index][start].rwcnt>=client->threshold[node_index])
	{
		if(!sa_entries[node_index][start].in_dram)
		{
			hot_nvm[node_index][hot_cnt[node_index]].nvm_addr=sa_entries[node_index][start].nvm_addr;
			hot_nvm[node_index][hot_cnt[node_index]].length=sa_entries[node_index][start].length;
			client->req_dram_size[node_index]+=hot_nvm[node_index][hot_cnt[node_index]].length;
			++hot_cnt[node_index];
		}
		--start;
	}
}

void dhmp_poll_ht_func(void)
{
	struct dhmp_addr_info *addr_info;
	int i,rw_cnt,node_index;
	struct dhmp_msg req_msg;
	struct itimerspec new_value;

	size_t cur_average_size=0;
	long long total_times,tmp_total_times;

	long long total_benefit_numerator[DHMP_SERVER_NODE_NUM],
			total_benefit_denominator[DHMP_SERVER_NODE_NUM];
	
	size_t dram_size_diff[DHMP_SERVER_NODE_NUM],
		dram_cache_size[DHMP_SERVER_NODE_NUM];
	int sa_index[DHMP_SERVER_NODE_NUM];
	double per_benefit=0.0,per_benefit_det=0.0,cur_hit_ratio=0.0,hit_ratio_det=0.0;

	
	INFO_LOG("------------poll hash table------------");
	
	for(i=0; i<DHMP_SERVER_NODE_NUM; i++)
	{
		sa_index[i]=hot_cnt[i]=cold_cnt[i]=0;

		/*set the diff between the request dram size and the response dram size*/
		dram_size_diff[i]=client->req_dram_size[i]-client->res_dram_size[i];
		client->req_dram_size[i]=client->res_dram_size[i]=0;

		if(dram_size_diff[i]>0)
			client->dram_threshold_policy[i]=false;
		else
		{
			if(client->dram_threshold_policy[i]==false)
				client->threshold[i]=5;
			client->dram_threshold_policy[i]=true;
		}
		
		if(client->threshold[i]==0)
			client->threshold[i]=1;
	}
	
	client->access_total_num=0;
	client->access_region_size=0;

	for(i=0;i<DHMP_CLIENT_HT_SIZE; i++)
	{
		if(hlist_empty(&client->addr_info_ht[i]))
			continue;
		
		hlist_for_each_entry(addr_info, &client->addr_info_ht[i], addr_entry)
		{
			node_index=addr_info->node_index;
			rw_cnt=addr_info->read_cnt+addr_info->write_cnt;
			if(rw_cnt==0)
				continue;
			max_cnt_test=max(rw_cnt, max_cnt_test);
			
			client->access_region_size+=rw_cnt*(addr_info->nvm_mr.length/1024);
			client->access_total_num+=rw_cnt;

			/*add nvm read latency ns*/
			total_benefit_denominator[node_index]+=
				addr_info->read_cnt*(DHMP_RTT_TIME+DHMP_DRAM_RW_TIME*addr_info->nvm_mr.length/1024+
				(addr_info->nvm_mr.length/PAGE_SIZE+1)*rdelay/knum);

			/*add nvm write latency ns*/
			total_benefit_denominator[node_index]+=
				addr_info->write_cnt*(DHMP_RTT_TIME+DHMP_DRAM_RW_TIME*addr_info->nvm_mr.length/1024+
				(addr_info->nvm_mr.length/PAGE_SIZE+1)*wdelay/knum);

			if(addr_info->dram_mr.addr!=NULL)
			{
				client->access_dram_num[node_index]+=rw_cnt;

				total_benefit_numerator[node_index]+=
					addr_info->read_cnt*((addr_info->nvm_mr.length/PAGE_SIZE+1)*rdelay/knum);
				
				total_benefit_numerator[node_index]+=
					addr_info->write_cnt*((addr_info->nvm_mr.length/PAGE_SIZE+1)*wdelay/knum);

				dram_cache_size[node_index]+=addr_info->nvm_mr.length/1024;
			}

			if(client->dram_threshold_policy[node_index])
			{
				if(rw_cnt>=client->threshold[node_index]&&addr_info->dram_mr.addr==NULL)
				{
					hot_nvm[node_index][hot_cnt[node_index]].nvm_addr=addr_info->nvm_mr.addr;
					hot_nvm[node_index][hot_cnt[node_index]].length=addr_info->nvm_mr.length;
					DEBUG_LOG("find hot addr %p",addr_info->nvm_mr.addr);
					client->req_dram_size[node_index]+=addr_info->nvm_mr.length;
					++hot_cnt[node_index];
				}
			}

			if(!client->dram_threshold_policy[node_index])
			{
				if((rw_cnt>=client->threshold[node_index])||
					(rw_cnt<client->threshold[node_index]&&addr_info->dram_mr.addr!=NULL))
				{
					sa_entries[node_index][sa_index[node_index]].nvm_addr=addr_info->nvm_mr.addr;
					sa_entries[node_index][sa_index[node_index]].length=addr_info->nvm_mr.length;
					sa_entries[node_index][sa_index[node_index]].rwcnt=rw_cnt;
					
					if(addr_info->dram_mr.addr==NULL)
						sa_entries[node_index][sa_index[node_index]].in_dram=false;
					else
						sa_entries[node_index][sa_index[node_index]].in_dram=true;
					
					++sa_index[node_index];
				}
			}
			
			addr_info->read_cnt=0;
			addr_info->write_cnt=0;
		}
		
	}

	client->poll_num=0;

	for(i=0; i<DHMP_SERVER_NODE_NUM; i++)
	{
		if(client->dram_threshold_policy[i]==false)
		{
			qsort(sa_entries[i], sa_index[i], sizeof(struct dhmp_sort_addr_entry), comp_sort_addr_entry);
			
			dhmp_handle_cache_model(i, sa_index[i]);
			
			if(cold_cnt[i]>0)
			{
				++client->poll_num;
				req_msg.data_size=cold_cnt[i]*sizeof(struct dhmp_nvm_info);
				req_msg.data=cold_nvm[i];
				req_msg.msg_type=DHMP_MSG_CLEAR_DRAM_REQUEST;
					
				dhmp_post_send(client->poll_trans[i], &req_msg);
			}
		}
		
		if(hot_cnt[i]>0)
		{
			++client->poll_num;
			req_msg.data_size=hot_cnt[i]*sizeof(struct dhmp_nvm_info);
			req_msg.data=hot_nvm[i];
			req_msg.msg_type=DHMP_MSG_APPLY_DRAM_REQUEST;

			dhmp_post_send(client->poll_trans[i], &req_msg);
		}
	}
	
	while(client->poll_num>0);

	for(i=0; i<DHMP_SERVER_NODE_NUM; i++)
	{
		if(client->dram_threshold_policy[i])
		{/*pool*/
			client->pre_hit_ratio[i]=-1.0;
			if(client->access_total_num!=0)
			{
				if(client->access_dram_num[i]==0)
					per_benefit=client->per_benefit[i];
				else
					per_benefit=(double)(((long double)total_benefit_numerator[i]
										/total_benefit_denominator[i])
										/dram_cache_size[i]);
				DEBUG_LOG("pre perbenefit %lf cur perbenefit %lf",client->per_benefit[i],per_benefit);
				if(client->per_benefit[i]!=0.0&&per_benefit!=0.0)
					per_benefit_det=(per_benefit-client->per_benefit[i])/per_benefit;
				else if(client->per_benefit[i]==0.0&&per_benefit!=0.0)
					per_benefit_det=(per_benefit-client->per_benefit_max[i])/per_benefit;
				else if(per_benefit==0.0)
					per_benefit_det=0.0;
				
				if(per_benefit_det>0.0&&per_benefit_det<1.0)
					client->threshold[i]=(int)(client->threshold[i]*(1-per_benefit_det)+0.5);
				DEBUG_LOG("perbenefit det %lf",per_benefit_det);	
				client->per_benefit[i]=per_benefit;
			}
			else 
				client->per_benefit[i]=0;
			
			if(client->per_benefit[i]!=0.0)
				client->per_benefit_max[i]=client->per_benefit[i];
		}
		else
		{/*cache*/
			
			if(client->access_total_num!=0)
			{
				if(client->pre_hit_ratio[i]<0.0)
				{
					cur_hit_ratio=((double)client->access_dram_num[i]/client->access_total_num);
					client->pre_hit_ratio[i]=cur_hit_ratio;
				}
				else
				{
					if(client->access_dram_num[i]==0)
						cur_hit_ratio=client->pre_hit_ratio[i];
					else
						cur_hit_ratio=((double)client->access_dram_num[i]/client->access_total_num);
				
					if(client->pre_hit_ratio[i]!=0.0&&cur_hit_ratio!=0.0)
						hit_ratio_det=(cur_hit_ratio-client->pre_hit_ratio[i])/cur_hit_ratio;
					else if(client->pre_hit_ratio[i]==0.0&&cur_hit_ratio!=0.0)
						hit_ratio_det=(cur_hit_ratio-client->pre_hit_ratio_max[i])/cur_hit_ratio;
					else if(cur_hit_ratio==0.0)
						hit_ratio_det=0.0;
					
					if(hit_ratio_det>0.0&&hit_ratio_det<1.0)
						client->threshold[i]=(int)(client->threshold[i]*(1-hit_ratio_det)+0.5);
				
					client->pre_hit_ratio[i]=cur_hit_ratio;
				}
				
			}
			else
				client->pre_hit_ratio[i]=0.0;

			if(client->pre_hit_ratio[i]!=0.0)
				client->pre_hit_ratio_max[i]=client->pre_hit_ratio[i];
		}
	}

	if(client->access_total_num==0)
		client->pre_average_size=0;
	else
	{
		cur_average_size=(size_t)(client->access_region_size/(double)client->access_total_num+0.5);
		if(client->pre_average_size==0)
		{
			total_times=DHMP_DEFAULT_POLL_TIME;
			tmp_total_times=(long long)(total_times*((long double)cur_average_size/DHMP_DEFAULT_SIZE));
		}
		else
		{
			total_times=client->poll_interval.tv_sec;
			total_times*=NANOSECOND;
			total_times+=client->poll_interval.tv_nsec;	
			tmp_total_times=(long long)(total_times*((long double)cur_average_size/client->pre_average_size));
		}
		client->pre_average_size=cur_average_size;
		client->poll_interval.tv_sec=tmp_total_times/NANOSECOND;
		client->poll_interval.tv_nsec=tmp_total_times%NANOSECOND;
	}
	
	new_value.it_value.tv_sec = client->poll_interval.tv_sec;
	new_value.it_value.tv_nsec = client->poll_interval.tv_nsec;
	new_value.it_interval.tv_sec = 0;
	new_value.it_interval.tv_nsec = 0;

	INFO_LOG("max cnt test %d", max_cnt_test);
	INFO_LOG("client sec %ld,nsec %ld",
			client->poll_interval.tv_sec, client->poll_interval.tv_nsec);
	timerfd_settime(client->poll_ht_fd, 0, &new_value, NULL);

	DEBUG_LOG("-------------------------------------------");
}

