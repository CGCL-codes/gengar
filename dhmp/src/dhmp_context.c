#include "dhmp.h"
#include "dhmp_log.h"
#include "dhmp_hash.h"
#include "dhmp_config.h"
#include "dhmp_context.h"

void *dhmp_context_run(void *data)
{
	struct epoll_event events[DHMP_EPOLL_SIZE];
	struct dhmp_event_data *event_data_ptr;
	struct dhmp_context *ctx=(struct dhmp_context*)data;
	int i,events_nr=0;
	
	ctx->stop=false;
	DEBUG_LOG("dhmp_context_run.");
	while(1)
	{	
		events_nr=epoll_wait(ctx->epoll_fd, events, ARRAY_SIZE(events), -1);
		
		if(events_nr>0)
		{
			for(i=0; i<events_nr; i++)
			{
				event_data_ptr=(struct dhmp_event_data*)events[i].data.ptr;
				event_data_ptr->event_handler(event_data_ptr->fd,
											event_data_ptr->data_ptr);
			}
		}
		
		if(ctx->stop)
			break;
	}
	
	DEBUG_LOG("dhmp_context stop.");
	return NULL;
}


int dhmp_context_init(struct dhmp_context *ctx)
{
	int retval=0;

	ctx->epoll_fd=epoll_create(DHMP_EPOLL_SIZE);
	if(ctx->epoll_fd < 0)
	{
		ERROR_LOG("create epoll fd error.");
		return -1;
	}

	retval=pthread_create(&ctx->epoll_thread, NULL, dhmp_context_run, ctx);
	if(retval)
	{
		ERROR_LOG("pthread create error.");
		close(ctx->epoll_fd);
	}

	return retval;
}

int dhmp_context_add_event_fd(struct dhmp_context *ctx,
							int events,
							int fd, 
							void *data,
							dhmp_event_handler event_handler)
{
	struct epoll_event ee;
	struct dhmp_event_data *event_data_ptr;
	int retval=0;
	
	event_data_ptr=(struct dhmp_event_data*)malloc(sizeof(struct dhmp_event_data));
	if(!event_data_ptr)
	{
		ERROR_LOG("allocate memory error.");
		return -1;
	}

	event_data_ptr->fd=fd;
	event_data_ptr->data_ptr=data;
	event_data_ptr->event_handler=event_handler;
	
	memset(&ee,0,sizeof(ee));
	ee.events=events;
	ee.data.ptr=event_data_ptr;

	retval=epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, fd, &ee);
	if(retval)
	{
		ERROR_LOG("dhmp_context add event fd error.");
		free(event_data_ptr);
	}
	else
		INFO_LOG("dhmp_context add event fd success.");
	
	return retval;
}

int dhmp_context_del_event_fd(struct dhmp_context *ctx, int fd)
{
	int retval=0;
	retval=epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	if(retval<0)
		ERROR_LOG("dhmp_context del event fd error.");
	
	return retval;
}

