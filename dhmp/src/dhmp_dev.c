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

/*
 * alloc rdma device resource
 */
static int dhmp_dev_init ( struct ibv_context* verbs, struct dhmp_device* dev_ptr )
{
	int retval=0;

	retval=ibv_query_device ( verbs, &dev_ptr->device_attr );
	if ( retval<0 )
	{
		ERROR_LOG ( "rdma query device attr error." );
		goto out;
	}
	
	dev_ptr->pd=ibv_alloc_pd ( verbs );
	if ( !dev_ptr->pd )
	{
		ERROR_LOG ( "allocate ibv_pd error." );
		retval=-1;
		goto out;
	}

	dev_ptr->verbs=verbs;
	INFO_LOG("max mr %d max qp mr %d max cqe %d",
			dev_ptr->device_attr.max_mr,
			dev_ptr->device_attr.max_qp_wr,
			dev_ptr->device_attr.max_cqe);
	return retval;

out:
	dev_ptr->verbs=NULL;
	return retval;
}

/*
 *	the function will get the rdma devices in the computer,
 * 	and init the rdma device, alloc the pd resource
 */
void dhmp_dev_list_init(struct list_head * dev_list_ptr)
{
	struct ibv_context** ctx_list;
	int i,num_devices=0,err=0;
	struct dhmp_device *dev_ptr;
	
	ctx_list=rdma_get_devices ( &num_devices );
	if ( !ctx_list )
	{
		ERROR_LOG ( "failed to get the rdma device list." );
		return ;
	}

	for ( i=0; i<num_devices; i++ )
	{
		if ( !ctx_list[i] )
		{
			ERROR_LOG ( "RDMA device [%d] is NULL.",i );
			continue;
		}
		
		dev_ptr=(struct dhmp_device*)malloc(sizeof(struct dhmp_device));
		if(!dev_ptr)
		{
			ERROR_LOG("allocate memory error.");
			break;
		}
		
		dhmp_dev_init ( ctx_list[i], dev_ptr );
		if ( !dev_ptr->verbs )
		{
			ERROR_LOG ( "RDMA device [%d]: name= %s allocate error.",
			            i, ibv_get_device_name ( ctx_list[i]->device ) );
		}
		else
		{
			INFO_LOG ( "RDMA device [%d]: name= %s allocate success.",
			           i, ibv_get_device_name ( ctx_list[i]->device ) );
			list_add_tail(&dev_ptr->dev_entry, dev_list_ptr);
		}
		
	}
	rdma_free_devices ( ctx_list );
}

/* 
 *	this function will clean the rdma resources
 *	include the rdma pd resource
 */
void dhmp_dev_list_destroy(struct list_head *dev_list_ptr)
{
	struct dhmp_device *dev_tmp_ptr, *dev_next_tmp_ptr;
	list_for_each_entry_safe(dev_tmp_ptr, 
						dev_next_tmp_ptr, 
						dev_list_ptr, 
						dev_entry)
	{
		if(dev_tmp_ptr->verbs)
			ibv_dealloc_pd ( dev_tmp_ptr->pd );
		free(dev_tmp_ptr);
	}
}

