#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "dhmp.h"
#include "dhmp_log.h"
#include "dhmp_hash.h"
#include "dhmp_config.h"

/*these strings in config xml*/
#define DHMP_DHMP_CONFIG_STR "dhmp_config"
#define DHMP_CLIENT_STR "client"
#define DHMP_LOG_LEVEL_STR "log_level"
#define DHMP_SERVER_STR "server"
#define DHMP_NIC_NAME_STR "nic_name"
#define DHMP_ADDR_STR "addr"
#define DHMP_PORT_STR "port"
#define DHMP_RDELAY_STR "rdelay"
#define DHMP_WDELAY_STR "wdelay"
#define DHMP_KNUM_STR "knum"

#define DHMP_WATCHER_STR "watcher"

static void dhmp_print_config ( struct dhmp_config* total_config_ptr )
{
	int i;
	for ( i=0; i<total_config_ptr->nets_cnt; i++ )
	{
		INFO_LOG ( "nic name %s",total_config_ptr->net_infos[i].nic_name );
		INFO_LOG ( "addr %s",total_config_ptr->net_infos[i].addr );
		INFO_LOG ( "port %d",total_config_ptr->net_infos[i].port );
		INFO_LOG ( "rdelay %d",total_config_ptr->simu_infos[i].rdelay );
		INFO_LOG ( "wdelay %d",total_config_ptr->simu_infos[i].wdelay );
		INFO_LOG ( "knum %d",total_config_ptr->simu_infos[i].knum );
	} 
}

static int dhmp_parse_watcher_node ( struct dhmp_config* config_ptr, int index, xmlDocPtr doc, xmlNodePtr curnode )
{
	xmlChar* val;
	int log_level=0;
	
	curnode=curnode->xmlChildrenNode;
	while(curnode!=NULL)
	{
		if(!xmlStrcmp(curnode->name, (const xmlChar*)DHMP_ADDR_STR))
		{
			val=xmlNodeListGetString ( doc, curnode->xmlChildrenNode, 1 );
			memcpy ( config_ptr->watcher_addr,
			    	( const void* ) val,strlen ( ( const char* ) val )+1 );
			xmlFree ( val );
		}

		if ( !xmlStrcmp ( curnode->name, ( const xmlChar* ) DHMP_PORT_STR ) )
		{
			val=xmlNodeListGetString ( doc, curnode->xmlChildrenNode, 1 );
			config_ptr->watcher_port=atoi ( ( const char* ) val );
			xmlFree ( val );
		}
		
		curnode=curnode->next;
	}

	return 0;
}

static int dhmp_parse_client_node ( struct dhmp_config* config_ptr, int index, xmlDocPtr doc, xmlNodePtr curnode )
{
	xmlChar* val;
	int log_level=0;
	
	curnode=curnode->xmlChildrenNode;
	while(curnode!=NULL)
	{
		if(!xmlStrcmp(curnode->name, (const xmlChar*)DHMP_LOG_LEVEL_STR))
		{
			val=xmlNodeListGetString ( doc, curnode->xmlChildrenNode, 1 );
			log_level=atoi ( ( const char* ) val );
			global_log_level=(enum dhmp_log_level)log_level;
			xmlFree ( val );
		}

		curnode=curnode->next;
	}

	return 0;
}

static int dhmp_parse_server_node ( struct dhmp_config* config_ptr, int index, xmlDocPtr doc, xmlNodePtr curnode )
{
	xmlChar* val;

	curnode=curnode->xmlChildrenNode;
	while ( curnode!=NULL )
	{
		if ( !xmlStrcmp ( curnode->name, ( const xmlChar* ) DHMP_NIC_NAME_STR ) )
		{
			val=xmlNodeListGetString ( doc, curnode->xmlChildrenNode, 1 );
			memcpy ( config_ptr->net_infos[index].nic_name,
			         ( const void* ) val,strlen ( ( const char* ) val )+1 );
			xmlFree ( val );
		}
	
		if ( !xmlStrcmp ( curnode->name, ( const xmlChar* ) DHMP_ADDR_STR ) )
		{
			val=xmlNodeListGetString ( doc, curnode->xmlChildrenNode, 1 );
			memcpy ( config_ptr->net_infos[index].addr,
			         ( const void* ) val,strlen ( ( const char* ) val )+1 );
			xmlFree ( val );
		}

		if ( !xmlStrcmp ( curnode->name, ( const xmlChar* ) DHMP_PORT_STR ) )
		{
			val=xmlNodeListGetString ( doc, curnode->xmlChildrenNode, 1 );
			config_ptr->net_infos[index].port=atoi ( ( const char* ) val );
			xmlFree ( val );
		}

		if ( !xmlStrcmp ( curnode->name, ( const xmlChar* ) DHMP_RDELAY_STR ) )
		{
			val=xmlNodeListGetString ( doc, curnode->xmlChildrenNode, 1 );
			config_ptr->simu_infos[index].rdelay=atoi ( ( const char* ) val );
			xmlFree ( val );
		}

		if ( !xmlStrcmp ( curnode->name, ( const xmlChar* ) DHMP_WDELAY_STR ) )
		{
			val=xmlNodeListGetString ( doc, curnode->xmlChildrenNode, 1 );
			config_ptr->simu_infos[index].wdelay=atoi ( ( const char* ) val );
			xmlFree ( val );
		}
		
		if ( !xmlStrcmp ( curnode->name, ( const xmlChar* ) DHMP_KNUM_STR ) )
		{
			val=xmlNodeListGetString ( doc, curnode->xmlChildrenNode, 1 );
			config_ptr->simu_infos[index].knum=atoi ( ( const char* ) val );
			xmlFree ( val );
		}

		curnode=curnode->next;
	}
	return 0;
}

static void dhmp_set_curnode_id ( struct dhmp_config* config_ptr )
{
	int socketfd, i, k, dev_num;
	char buf[BUFSIZ];
	const char* addr;
	struct ifconf conf;
	struct ifreq* ifr;
	struct sockaddr_in* sin;
	bool res=false;

	socketfd = socket ( PF_INET, SOCK_DGRAM, 0 );
	conf.ifc_len = BUFSIZ;
	conf.ifc_buf = buf;

	ioctl ( socketfd, SIOCGIFCONF, &conf );
	dev_num = conf.ifc_len / sizeof ( struct ifreq );
	ifr = conf.ifc_req;

	for(k=0; k<config_ptr->nets_cnt; k++)
	{
		for ( i=0; i < dev_num; i++ )
		{
			sin = ( struct sockaddr_in* ) ( &ifr->ifr_addr );

			ioctl ( socketfd, SIOCGIFFLAGS, ifr );
			if ( ( ( ifr->ifr_flags & IFF_LOOPBACK ) == 0 ) && 
					( ifr->ifr_flags & IFF_UP ) &&
					( strcmp ( ifr->ifr_name, config_ptr->net_infos[k].nic_name ) ==0 ) )
			{
				INFO_LOG ( "%s %s", ifr->ifr_name, inet_ntoa ( sin->sin_addr ) );
				addr=inet_ntoa ( sin->sin_addr );
				break;
			}
			ifr++;
		}

		if ( i!=dev_num )
		{
			for ( i=0; i<config_ptr->nets_cnt; i++ )
			{
				if ( !strcmp ( config_ptr->net_infos[i].addr, addr ) )
				{
					config_ptr->curnet_id=i;
					res=true;
					break;
				}
			}
		}

		if(res)
			break;
	}

	if(res)
		INFO_LOG("curnode server id is %d",config_ptr->curnet_id);
	else
	{
		ERROR_LOG("server addr in config.xml is error.");
		exit(-1);
	}
}

int dhmp_config_init ( struct dhmp_config* config_ptr, bool is_client )
{
	const char* config_file=DHMP_CONFIG_FILE_NAME;
	int index=0;
	xmlDocPtr config_doc;
	xmlNodePtr curnode;

	config_doc=xmlParseFile ( config_file );
	if ( !config_doc )
	{
		ERROR_LOG ( "xml parse file error." );
		return -1;
	}

	curnode=xmlDocGetRootElement ( config_doc );
	if ( !curnode )
	{
		ERROR_LOG ( "xml doc get root element error." );
		goto cleandoc;
	}

	if ( xmlStrcmp ( curnode->name, ( const xmlChar* ) DHMP_DHMP_CONFIG_STR ) )
	{
		ERROR_LOG ( "xml root node is not dhmp_config string." );
		goto cleandoc;
	}

	config_ptr->nets_cnt=0;
	config_ptr->curnet_id=-1;
	curnode=curnode->xmlChildrenNode;
	while ( curnode )
	{
		if ( !xmlStrcmp ( curnode->name, ( const xmlChar* ) DHMP_WATCHER_STR ) )
			dhmp_parse_watcher_node ( config_ptr, index, config_doc, curnode );
		
		if ( !xmlStrcmp ( curnode->name, ( const xmlChar* ) DHMP_CLIENT_STR ) )
			dhmp_parse_client_node ( config_ptr, index, config_doc, curnode );
		
		if ( !xmlStrcmp ( curnode->name, ( const xmlChar* ) DHMP_SERVER_STR ) )
		{
			dhmp_parse_server_node ( config_ptr, index, config_doc, curnode );
			config_ptr->nets_cnt++;
			index++;
		}
		curnode=curnode->next;
	}

	dhmp_print_config ( config_ptr );
	xmlFreeDoc ( config_doc );
	if ( !is_client )
		dhmp_set_curnode_id ( config_ptr );
	return 0;

cleandoc:
	xmlFreeDoc ( config_doc );
	return -1;
}

