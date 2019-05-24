#include <stdio.h>

#include "dhmp.h"

int num[32];
int cnt=0;
void *addr[32];

void swp(int i, int j)
{
	int tmp=num[i];
	num[i]=num[j];
	num[j]=tmp;
}

void perm(int start)
{
	int i;
	if(start>=31)
	{
		for(i=0;i<32;i++)
			addr[i]=dhmp_malloc(4096);

		
		for(i=0;i<32;i++)
			dhmp_free(addr[num[i]]);
		cnt++;
		if(cnt%100==0)
			printf("%d ",cnt);
	}
	else
	{
		
		for(i=start;i<32;i++)
		{
			swp(start, i);
			perm(start+1);
			swp(start, i);
		}
	}
}

int main()
{
	int i;
	
	for(i=0;i<32;i++)
		num[i]=i;
	
	dhmp_client_init();

	perm(0);
	
	dhmp_client_destroy();
	
	return 0;
}

