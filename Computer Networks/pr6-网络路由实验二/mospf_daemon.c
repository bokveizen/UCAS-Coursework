#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"
#include "arp.h"
#include "packet.h"
#include "list.h"
#include "log.h"
#include "rtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;

#define fmt(addr) addr>>24, addr<<8>>24, addr<<16>>24, addr<<24>>24
#define RTB_UPDT_FRQC 8

//访问新节点后的邻居节点更新
void nbr_update(int index, int rt_count, graph_t graph_node[])
{
	u32 visit_id = graph_node[index].rid;
	u32 visit_gw = graph_node[index].gw;
	mospf_db_entry_t *entry = NULL;
	int dist_via_visit_node = graph_node[index].dist + 1;
	iface_info_t *iface_t = graph_node[index].send_iface;

	list_for_each_entry(entry, &mospf_db, list)
	{
		if(entry->rid == visit_id)
		{
			for(int i = 0; i < entry->nadv; i++)
			{
				if((entry->array)[i].rid == instance->router_id)
                //访问节点的邻居节点是源节点 忽略
					continue;
				for(int j = 0; j < rt_count; j++)
				{
					if(graph_node[j].rid == (entry->array)[i].rid)
                    {//源节点经过访问节点距离更近 更新graph_node
                    	if(dist_via_visit_node < graph_node[j].dist)
                    	{
                    		graph_node[j].dist = dist_via_visit_node;
                    		graph_node[j].send_iface = iface_t;
                    		graph_node[j].gw = visit_gw;
                    	}
                    	else 
                    		break;
                    }
                    else if(graph_node[j].rid == -1)
                    {//没有访问节点的邻居节点的graph_node 新建
                    	graph_node[j].rid = (entry->array)[i].rid;
                    	graph_node[j].dist = dist_via_visit_node;
                    	graph_node[j].send_iface = iface_t;
                    	graph_node[j].gw = visit_gw;
                    }
                }
            }
        }
    }
}

int set_visited(int rt_count, graph_t graph_node[])
{
	int min_dist = 65536;
	int index = 0;
	for(int i = 0; i < rt_count; i++)
	{
		if(graph_node[i].dist < min_dist && !graph_node[i].visited)
        {//找未访问路由节点中距离源节点最近的节点
        	printf("找未访问路由节点中距离源节点最近的节点\n");
        	min_dist = graph_node[i].dist;
        	index = i;
        }
    }
    printf("将新访问的路由节点信息填入路由表\n");
    printf("index = %d\n", index);
    //将新访问的路由节点信息填入路由表
    graph_node[index].visited = 1;
    mospf_db_entry_t *entry = NULL;
    list_for_each_entry(entry, &mospf_db, list)
    {
    	printf("in list_for_each_entry\n");
    	if(entry->rid == graph_node[index].rid)
    	{
    		for(int i = 0; i < entry->nadv; i++)
    		{
    			printf("%d say hello\n", i);
    			if(longest_prefix_match((entry->array)[i].subnet))
    				continue;
    			else
    			{
    				printf("%d say else gogo\n", i);
    				rt_entry_t *entry_t = new_rt_entry((entry->array)[i].subnet, (entry->array)[i].mask, graph_node[index].gw, graph_node[index].send_iface);
    				add_rt_entry(entry_t);
    			}            
    		}
    	}
    }
    return index;
}

//初始化图
void initial_graph(int rt_count, graph_t graph_node[])
{
	for(int i = 0; i < rt_count; i++)
	{
		graph_node[i].rid = -1;
		graph_node[i].dist = 65535;
		graph_node[i].send_iface = NULL;
		graph_node[i].visited = 0;
		graph_node[i].gw = 0;
	}
}

void *rtable_update(void *param)
{
	while(1)
	{
		sleep(RTB_UPDT_FRQC);

    	//router counting
		int rt_count = 0;
		pthread_mutex_lock(&mospf_lock);
		clear_rtable();
		mospf_db_entry_t *entry = NULL;
		list_for_each_entry(entry, &mospf_db, list) {
			rt_count++;
		}
		graph_t graph_node[rt_count];
		initial_graph(rt_count, graph_node);
		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &instance->iface_list, list)
		{
			mospf_nbr_t *nbr = NULL;
			list_for_each_entry(nbr, &(iface->nbr_list), list)
			{
				for(int i = 0; i < rt_count; i++)
				{
					if(graph_node[i].rid == -1)
					{
						graph_node[i].rid = nbr->nbr_id;
						graph_node[i].dist = 1;
						graph_node[i].send_iface = iface;
						graph_node[i].gw = nbr->nbr_ip;
						break;
					}
				}
			}
			printf("先将源节点的所有端口的IP填入路由表\n");
            //先将源节点的所有端口的IP填入路由表
			rt_entry_t *entry_t = new_rt_entry((iface->ip & iface->mask), iface->mask, 0, iface);
			add_rt_entry(entry_t);
			printf("OK!\n");
		}

		for(int i = 0; i < rt_count; i++)
		{
			printf("rt_count = %d\n", i);
			printf("set_visited\n");
			int num = set_visited(rt_count, graph_node);
			printf("set_visited OK nbr_update gogo\n");
			nbr_update(num, rt_count, graph_node);
			printf("nbr_update OK!\n");
		}
		pthread_mutex_unlock(&mospf_lock);
		print_rtable();
	}
}

/*
生成并洪泛链路状态
当节点的邻居列表发生变动时，或者超过lsu interval (30 sec)未发送过链路状态信息时
向每个邻居节点发送链路状态信息
包含该节点ID (mOSPF Header)、邻居节点ID、网络和掩码 (mOSPF LSU)
序列号(sequence number)，每次生成链路状态信息时加1
目的IP地址为邻居节点相应端口的IP地址，目的MAC地址为邻居节点相应端口的MAC地址
*/
void lsu_update()
{
	u32 nadv = 0;
	struct mospf_lsa* lsa = NULL;
	iface_info_t *iface = NULL;
	pthread_mutex_lock(&mospf_lock);
	list_for_each_entry(iface, &instance->iface_list, list)
	{
		mospf_nbr_t *nbr_t = NULL;
		int not_empty = 0;
		list_for_each_entry(nbr_t, &(iface->nbr_list), list)
		{
			nadv++;
			not_empty = 1;
		}
		if(!not_empty) nadv++;
	}	
	int count = 0;
	if(nadv)
	{
		lsa = (struct mospf_lsa*)malloc(nadv * sizeof(struct mospf_lsa));
		iface_info_t *iface_t = NULL;
		list_for_each_entry(iface_t, &instance->iface_list, list)
		{
			int not_empty = 0;
			mospf_nbr_t *nbr_t = NULL;
			list_for_each_entry(nbr_t, &(iface_t->nbr_list), list)
			{
				lsa[count].subnet = htonl((nbr_t->nbr_mask) & (nbr_t->nbr_ip));
				lsa[count].mask = htonl(nbr_t->nbr_mask);
				lsa[count].rid = htonl(nbr_t->nbr_id);
				count++;
				not_empty = 1;
			}
			if(!not_empty)
			{
				lsa[count].subnet = htonl((iface_t->ip) & (iface_t->mask));
				lsa[count].mask = htonl(iface_t->mask);				
				lsa[count].rid = htonl((u32)0);
				count++;
			}
		}			
	}
	pthread_mutex_unlock(&mospf_lock);
	
	iface_info_t* iface_t = NULL;
	list_for_each_entry(iface_t, &instance->iface_list, list)
	{
		mospf_nbr_t *nbr_t = NULL;
		list_for_each_entry(nbr_t, &(iface_t->nbr_list),list)
		{
			char* pkt;
			int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nadv * MOSPF_LSA_SIZE;
			pkt = (char*)malloc(len);
			memset(pkt, 0, len);
			struct ether_header* eth_hdr = (struct ether_header*)(pkt);	
			struct iphdr* ip_hdr = (struct iphdr *)(pkt + ETHER_HDR_SIZE);
			for(int i = 0; i < ETH_ALEN; ++i) eth_hdr->ether_shost[i] = iface_t->mac[i];
				ip_init_hdr(ip_hdr, iface_t->ip, nbr_t->nbr_ip, len - ETHER_HDR_SIZE, IPPROTO_MOSPF);
			struct mospf_hdr *mospf_hdr = (struct mospf_hdr *)(pkt + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
			struct mospf_lsu *mospf_lsu = (struct mospf_lsu *)(pkt + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE);
			struct mospf_lsa *mospf_lsa = (struct mospf_lsa *)(pkt + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE);
			mospf_init_hdr(mospf_hdr, MOSPF_TYPE_LSU, MOSPF_HDR_SIZE, instance->router_id, instance->area_id);	
			mospf_init_lsu(mospf_lsu, nadv);
			instance->sequence_num++;
			if(nadv) memcpy(mospf_lsa, (char*)lsa, nadv * MOSPF_LSA_SIZE);
			mospf_hdr->checksum = mospf_checksum(mospf_hdr);
			iface_send_packet_by_arp(iface_t, nbr_t->nbr_ip, pkt, len);
		}		
	}
	//if(nadv) free(lsa);
}

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);

	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);

void mospf_run()
{
	pthread_t hello, lsu, nbr, rtb;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&rtb, NULL, rtable_update, NULL);
}

/*
每个节点周期性的（5秒）广播自己
发送mOSPF Hello消息，包括节点ID, 端口的子网掩码
目的IP地址为224.0.0.5，目的MAC地址为01:00:5E:00:00:05
*/
void *sending_mospf_hello_thread(void *param)
{
	while(1)
	{
		char* pkt;
		u16 tot_size = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE;
		pkt = (char*)malloc(tot_size);
		struct ether_header* eth_hdr = (struct ether_header*)(pkt);
		eth_hdr->ether_dhost[0] = 0x01;
		eth_hdr->ether_dhost[1] = 0x00;
		eth_hdr->ether_dhost[2] = 0x5e;
		eth_hdr->ether_dhost[3] = 0x00;
		eth_hdr->ether_dhost[4] = 0x00;
		eth_hdr->ether_dhost[5] = 0x05;
		eth_hdr->ether_type = htons(ETH_P_IP);

		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &instance->iface_list, list)
		{
			for (int i = 0; i < ETH_ALEN; ++i)
			{
				eth_hdr->ether_shost[i] = iface->mac[i];
			}

			struct iphdr* ip_hdr = (struct iphdr*)(pkt + ETHER_HDR_SIZE);
			u16 iphdr_size = tot_size - ETHER_HDR_SIZE;
			ip_init_hdr(ip_hdr, iface->ip, MOSPF_ALLSPFRouters, iphdr_size, IPPROTO_MOSPF);	
			ip_hdr->ttl = 1;

			struct mospf_hdr* mospf_hdr = (struct mospf_hdr*)(pkt + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
			mospf_init_hdr(mospf_hdr, MOSPF_TYPE_HELLO, MOSPF_HDR_SIZE, instance->router_id, 0);		

			struct mospf_hello* mospf_hello = (struct mospf_hello*)(pkt + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE);	
			mospf_init_hello(mospf_hello, iface->mask);
			mospf_hdr->checksum = mospf_checksum(mospf_hdr);
			_iface_send_packet(iface, pkt, tot_size);
		}
		sleep(5);
	}

	return NULL;
}

void *checking_nbr_thread(void *param)
{
	while(1)
	{
		iface_info_t *iface = NULL;
		int found = 0;
		pthread_mutex_lock(&mospf_lock);
		list_for_each_entry(iface, &instance->iface_list, list)
		{
			mospf_nbr_t *entry = NULL;
			list_for_each_entry(entry, &(iface->nbr_list),list)
			{
				if(entry->alive) entry->alive--;
				else
				{
					found = 1;
					list_delete_entry(&(entry->list));
				}
			}
		}
		pthread_mutex_unlock(&mospf_lock);		
		if(found) lsu_update();
		sleep(1);
	}

	return NULL;
}

/*
节点收到mOSPF Hello消息后
如果发送该消息的节点不在邻居列表中，添加至邻居列表
如果已存在，更新其达到时间
*/
void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
	struct iphdr *ip_hdr = (struct iphdr*)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf_hdr = (struct mospf_hdr*)((char*)ip_hdr + IP_HDR_SIZE(ip_hdr));
	struct mospf_hello *mospf_hello = (struct mospf_hello*)((char*)mospf_hdr + MOSPF_HDR_SIZE);
	
	mospf_nbr_t *entry = NULL, *existed = NULL;
	pthread_mutex_lock(&mospf_lock);
	list_for_each_entry(entry, &(iface->nbr_list), list)
	{
		if(entry->nbr_id == ntohl(mospf_hdr->rid))
		{
			existed = entry;
			break;
		}
	}
	pthread_mutex_unlock(&mospf_lock);
	if(!existed)
	{
		pthread_mutex_lock(&mospf_lock);
		iface->num_nbr++;
		mospf_nbr_t* new_nbr;
		new_nbr = (mospf_nbr_t*)malloc(sizeof(mospf_nbr_t));
		new_nbr->nbr_id = ntohl(mospf_hdr->rid);
		new_nbr->nbr_ip = ntohl(ip_hdr->saddr);
		new_nbr->nbr_mask = ntohl(mospf_hello->mask);
		new_nbr->alive = (u8)(3*ntohs(mospf_hello->helloint));
		list_add_tail(&(new_nbr->list), &(iface->nbr_list));
		pthread_mutex_unlock(&mospf_lock);
		lsu_update();
	}
	else{
		pthread_mutex_lock(&mospf_lock);
		existed->alive = (u8)(3*ntohs(mospf_hello->helloint));
		pthread_mutex_unlock(&mospf_lock);
	}
	//free(packet);
}

void *sending_mospf_lsu_thread(void *param)
{
	while(1)
	{
		lsu_update();
		sleep(30);
	}
	return NULL;
}

/*
收到链路状态信息后
如果之前未收到该节点的链路状态信息，或者该信息的序列号更大，则更新链路状态数据库
TTL减1，如果TTL值大于0，则向除该端口以外的端口转发该消息
*/
void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip_hdr = (struct iphdr*)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf_hdr = (struct mospf_hdr*)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
	struct mospf_lsu *mospf_lsu = (struct mospf_lsu*)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE);
	struct mospf_lsa *mospf_lsa = (struct mospf_lsa*)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE);
	if(ntohl(mospf_hdr->rid) != instance->router_id)
	{
		pthread_mutex_lock(&mospf_lock);
		mospf_db_entry_t *entry = NULL, *existed = NULL;
		list_for_each_entry(entry, &mospf_db, list)
		{
			if(entry->rid == ntohl(mospf_hdr->rid))
			{
				existed = entry;
				break;
			}
		}
		if(!existed)
		{
			mospf_db_entry_t* new_db;
			new_db = (mospf_db_entry_t*)malloc(sizeof(mospf_db_entry_t));
			new_db->rid = ntohl(mospf_hdr->rid);
			new_db->seq = ntohs(mospf_lsu->seq);
			new_db->nadv = ntohl(mospf_lsu->nadv);
			struct mospf_lsa* new_lsa;
			new_lsa = (struct mospf_lsa*)malloc((new_db->nadv) * sizeof(struct mospf_lsa));
			for(int i = 0; i < new_db->nadv; ++i)
			{
				new_lsa[i].subnet = ntohl(mospf_lsa[i].subnet);
				new_lsa[i].mask = ntohl(mospf_lsa[i].mask);
				new_lsa[i].rid = ntohl(mospf_lsa[i].rid);
			}
			new_db->array = new_lsa;
			list_add_tail(&(new_db->list), &mospf_db);
			//db_print();
		}
		else if(existed->seq < ntohs(mospf_lsu->seq))
		{
			existed->rid = ntohl(mospf_hdr->rid);
			existed->seq = ntohs(mospf_lsu->seq);
			existed->nadv = ntohl(mospf_lsu->nadv);
			struct mospf_lsa* new_lsa;
			new_lsa = (struct mospf_lsa*)malloc((existed->nadv)*sizeof(struct mospf_lsa));		
			for(int i = 0; i < existed->nadv; ++i)
			{
				new_lsa[i].subnet = ntohl(mospf_lsa[i].subnet);
				new_lsa[i].mask = ntohl(mospf_lsa[i].mask);
				new_lsa[i].rid = ntohl(mospf_lsa[i].rid);
			}	
			existed->array = new_lsa;
			//db_print();
		}
		pthread_mutex_unlock(&mospf_lock);
	}
	mospf_lsu->ttl--;
	if(mospf_lsu->ttl)
	{
		iface_info_t *iface_t = NULL;
		list_for_each_entry(iface_t, &instance->iface_list, list)
		{
			if(iface_t->index == iface->index) continue;
			mospf_nbr_t *nbr_t = NULL;
			char* pkt;
			pkt = (char*)malloc(len);
			memcpy(pkt, packet, len);
			struct ether_header* ethh = (struct ether_header*)(pkt);
			struct iphdr *iph = (struct iphdr *)(pkt + ETHER_HDR_SIZE);
			list_for_each_entry(nbr_t, &(iface_t->nbr_list), list)
			{
				for(int i = 0; i < ETH_ALEN; ++i) ethh->ether_shost[i] = iface_t->mac[i];
					ip_init_hdr(iph, ntohl(iph->saddr), nbr_t->nbr_ip, ntohs(iph->tot_len), iph->protocol);
				iface_send_packet_by_arp(iface_t, nbr_t->nbr_ip, pkt, len);
			}			
		}
		//free(packet);
	}
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

	if (mospf->version != MOSPF_VERSION) {
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return ;
	}
	if (mospf->checksum != mospf_checksum(mospf)) {
		log(ERROR, "received mospf packet with incorrect checksum");
		return ;
	}
	if (ntohl(mospf->aid) != instance->area_id) {
		log(ERROR, "received mospf packet with incorrect area id");
		return ;
	}

	// log(DEBUG, "received mospf packet, type: %d", mospf->type);

	switch (mospf->type) {
		case MOSPF_TYPE_HELLO:
		handle_mospf_hello(iface, packet, len);
		break;
		case MOSPF_TYPE_LSU:
		handle_mospf_lsu(iface, packet, len);
		break;
		default:
		log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
		break;
	}
}


