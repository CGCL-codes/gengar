#include <stdio.h>
#include "dhmp.h"

int main(int argc,char *argv[])
{
	void *addr;
	int size=65536;
	char *str;

	str=malloc(size);
	snprintf(str, size, "hello world");
	
	dhmp_client_init();

	addr=dhmp_malloc(size);
	
	dhmp_write(addr, str, size);

	dhmp_read(addr, str, size);
	
	dhmp_free(addr);
	
	dhmp_client_destroy();

	return 0;
}



