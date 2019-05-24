#include "dhmp.h"
#include "dhmp_log.h"
#include "dhmp_hash.h"
#include "dhmp_config.h"
#include "dhmp_context.h"
#include "dhmp_dev.h"
#include "dhmp_transport.h"
#include "dhmp_task.h"
#include "dhmp_timerfd.h"

int dhmp_timerfd_create(struct itimerspec *new_value)
{
	int tfd,err=0;

	tfd=timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if(tfd<0)
	{
		ERROR_LOG("create timerfd error.");
		return -1;
	}

	err=timerfd_settime(tfd, 0, new_value, NULL);
	if(err)
	{
		ERROR_LOG("timerfd settime error.");
		return err;
	}
	
	return tfd;
}

