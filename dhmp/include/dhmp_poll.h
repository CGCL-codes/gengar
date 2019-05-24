#ifndef DHMP_POLL_H
#define DHMP_POLL_H

struct dhmp_nvm_info
{
	void *nvm_addr;
	size_t length;
};

struct dhmp_sort_addr_entry{
	void *nvm_addr;
	size_t length;
	int rwcnt;
	bool in_dram;
};

void dhmp_poll_ht_func(void);

#endif
