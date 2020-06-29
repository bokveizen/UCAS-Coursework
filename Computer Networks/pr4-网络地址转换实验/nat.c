#include "nat.h"
#include "ip.h"
#include "icmp.h"
#include "tcp.h"
#include "rtable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static struct nat_table nat;

u16 free_port_index_get()
{
	for(int i = NAT_PORT_MIN; i <= NAT_PORT_MAX; i++)
	{
		if(nat.assigned_ports[i] == 0)
		{
			nat.assigned_ports[i] = 1;
			return i;
		}
	}
	printf("No free port!!\n");
	return 0;
}

// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name)
{
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (strcmp(iface->name, if_name) == 0)
			return iface;
	}

	log(ERROR, "Could not find the desired interface according to if_name '%s'", if_name);
	return NULL;
}

// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
static int get_packet_direction(char *packet)
{
	struct iphdr* iphdr = packet_to_ip_hdr(packet);
	u32 src_ip = ntohl(iphdr->saddr);
	u32 dst_ip = ntohl(iphdr->daddr);
	rt_entry_t* src_entry = longest_prefix_match(src_ip);
	rt_entry_t* dst_entry = longest_prefix_match(dst_ip);

	if(src_entry->iface->ip == nat.internal_iface->ip 
		&& dst_entry->iface->ip == nat.external_iface->ip) return DIR_OUT;
	//当源地址为内部地址，且目的地址为外部地址时，方向为DIR_OUT

	if(src_entry->iface->ip == nat.external_iface->ip 
		&& dst_ip == nat.external_iface->ip) return DIR_IN;
	//当源地址为外部地址，且目的地址为external_iface地址时，方向为DIR_IN

	return DIR_INVALID;
}

// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
void do_translation(iface_info_t *iface, char *packet, int len, int dir)
{
	struct iphdr* iphdr = packet_to_ip_hdr(packet);
	struct tcphdr* tcphdr = (struct tcphdr*)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(iphdr));
	char* buf;
	buf = (char*)malloc(6*sizeof(char));
	struct nat_mapping* nat_entry = NULL;

	if(dir == DIR_IN)
	{
		u32 dst_ip = ntohl(iphdr->daddr);
		u16 dst_port = ntohs(tcphdr->dport);
		u32 srv_ip = ntohl(iphdr->saddr);
		u16 srv_port = ntohs(tcphdr->sport);
		memcpy(buf,(char*)(&srv_ip), 4);
		memcpy(buf + 4, (char*)(&srv_port), 2);
		u8 index = hash8(buf, 6);
		struct nat_mapping* nat_temp;
		list_for_each_entry_safe(nat_entry, nat_temp, &(nat.nat_mapping_list[index]), list)
		//查找映射关系，进行(internal_ip, internal_port) <-> (external_ip, external_port)之间的转换
		{
			if(dst_ip == nat_entry->external_ip && dst_port == nat_entry->external_port)
			{
				//更新IP/TCP数据包头部字段(包括校验和)
				iphdr->daddr = htonl(nat_entry->internal_ip);
				tcphdr->dport = htons(nat_entry->internal_port);
				iphdr->checksum = ip_checksum(iphdr);
				tcphdr->checksum = tcp_checksum(iphdr, tcphdr);
				nat_entry->update_time = time(NULL);
				if(tcphdr->flags == TCP_FIN)
					nat_entry->conn.external_fin = TCP_FIN;
				if(ntohl(tcphdr->seq) > nat_entry->conn.external_seq_end)
					nat_entry->conn.external_seq_end = ntohl(tcphdr->seq);
				if(ntohl(tcphdr->ack) > nat_entry->conn.external_ack)
					nat_entry->conn.external_ack = ntohl(tcphdr->ack);
				break;
			}
		}
	}
	else //dir == DIR_OUT
	{
		u32 srv_ip = ntohl(iphdr->daddr);
		u16 srv_port = ntohs(tcphdr->dport);
		u32 src_ip = ntohl(iphdr->saddr);
		u16 src_port = ntohs(tcphdr->sport);
		memcpy(buf,(char*)(&srv_ip), 4);
		memcpy(buf + 4, (char*)(&srv_port), 2);
		u8 index = hash8(buf, 6);
		struct nat_mapping* nat_temp;
		int found = 0;
		list_for_each_entry_safe(nat_entry, nat_temp, &(nat.nat_mapping_list[index]), list)
		//查找映射关系，进行(internal_ip, internal_port) <-> (external_ip, external_port)之间的转换
		{
			if(src_ip == nat_entry->internal_ip && src_port == nat_entry->internal_port)
			{
				found = 1;
				//更新IP/TCP数据包头部字段(包括校验和)
				iphdr->saddr = htonl(nat_entry->external_ip);
				tcphdr->sport = htons(nat_entry->external_port);
				iphdr->checksum = ip_checksum(iphdr);
				tcphdr->checksum = tcp_checksum(iphdr, tcphdr);
				nat_entry->update_time = time(NULL);
				if(tcphdr->flags == TCP_FIN)
					nat_entry->conn.internal_fin = TCP_FIN;
				if(ntohl(tcphdr->seq) > nat_entry->conn.internal_seq_end)
					nat_entry->conn.internal_seq_end = ntohl(tcphdr->seq);
				if(ntohl(tcphdr->ack) > nat_entry->conn.internal_ack)
					nat_entry->conn.internal_ack = ntohl(tcphdr->ack);
				break;
			}
		}
		if(!found)
		//如果为DIR_OUT方向数据包且没有对应连接映射
		{
			struct nat_mapping* insert_temp;
			insert_temp = (struct nat_mapping*)malloc(sizeof(struct nat_mapping));
			insert_temp->internal_ip = src_ip;
			insert_temp->internal_port = src_port;
			insert_temp->external_ip = nat.external_iface->ip;
			insert_temp->external_port = free_port_index_get();
			insert_temp->update_time = time(NULL);
			memset(&(insert_temp->conn), 0 ,sizeof(struct nat_connection));
			if(tcphdr->flags == TCP_FIN)
				insert_temp->conn.internal_fin = TCP_FIN;
			if(ntohl(tcphdr->seq) > insert_temp->conn.internal_seq_end)
				insert_temp->conn.internal_seq_end = ntohl(tcphdr->seq);
			if(ntohl(tcphdr->ack) > insert_temp->conn.internal_ack)
				insert_temp->conn.internal_ack = ntohl(tcphdr->ack);			
			list_add_tail(&(insert_temp->list), &(nat.nat_mapping_list[index]));

			iphdr->saddr = htonl(insert_temp->external_ip);
			tcphdr->sport = htons(insert_temp->external_port);
			iphdr->checksum = ip_checksum(iphdr);
			tcphdr->checksum = tcp_checksum(iphdr, tcphdr);			
		}
		
	}
	ip_send_packet(packet, len);
	if(tcphdr->flags == TCP_RST)
	{
		if(nat_entry == NULL)
		{
			printf("nat_entry EMPTY!!\n");
			return;
		}
		pthread_mutex_lock(&nat.lock);
		u16 free_port = nat_entry->external_port; 
		nat.assigned_ports[free_port] = 0;
		list_delete_entry(&(nat_entry->list));
		free(nat_entry);
		pthread_mutex_unlock(&nat.lock);
	}
}

void nat_translate_packet(iface_info_t *iface, char *packet, int len)
{
	int dir = get_packet_direction(packet);
	if (dir == DIR_INVALID) {
		log(ERROR, "invalid packet direction, drop it.");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return ;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	if (ip->protocol != IPPROTO_TCP) {
		log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
		free(packet);
		return ;
	}

	do_translation(iface, packet, len, dir);
}

// nat timeout thread: find the finished flows, remove them and free port
// resource
void *nat_timeout()
{
	while (1) {
		struct nat_mapping* nat_entry_t = NULL;
		struct nat_mapping* nat_temp_t;
		pthread_mutex_lock(&nat.lock);
		for(int i = 0; i < HASH_8BITS; i++)
		{
			list_for_each_entry_safe(nat_entry_t, nat_temp_t, &(nat.nat_mapping_list[i]), list)
			{
				if( (nat_entry_t->conn.internal_fin == TCP_FIN 
					&& nat_entry_t->conn.external_fin == TCP_FIN)
					//双方都已发送FIN且回复相应ACK的连接，一方发送RST包的连接 
					|| (time(NULL) - nat_entry_t->update_time > 60) )
					//双方已经超过60秒未传输数据的连接
				{
					u16 free_port = nat_entry_t->external_port; 
					nat.assigned_ports[free_port] = 0;					
					list_delete_entry(&(nat_entry_t->list));
					free(nat_entry_t);
				}
			}
		}
		pthread_mutex_unlock(&nat.lock);
		sleep(1);
	}
	return NULL;
}

// initialize nat table
void nat_table_init()
{
	memset(&nat, 0, sizeof(nat));

	for (int i = 0; i < HASH_8BITS; i++)
		init_list_head(&nat.nat_mapping_list[i]);

	nat.internal_iface = if_name_to_iface("n1-eth0");
	nat.external_iface = if_name_to_iface("n1-eth1");
	if (!nat.internal_iface || !nat.external_iface) {
		log(ERROR, "Could not find the desired interfaces for nat.");
		exit(1);
	}

	memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));

	pthread_mutex_init(&nat.lock, NULL);

	pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

// destroy nat table
void nat_table_destroy()
{
	pthread_mutex_lock(&nat.lock);

	for (int i = 0; i < HASH_8BITS; i++) {
		struct list_head *head = &nat.nat_mapping_list[i];
		struct nat_mapping *mapping_entry, *q;
		list_for_each_entry_safe(mapping_entry, q, head, list) {
			list_delete_entry(&mapping_entry->list);
			free(mapping_entry);
		}
	}

	pthread_kill(nat.thread, SIGTERM);

	pthread_mutex_unlock(&nat.lock);
}

