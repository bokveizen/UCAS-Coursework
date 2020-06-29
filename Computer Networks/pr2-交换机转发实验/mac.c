#include "mac.h"
#include "headers.h"
#include "log.h"

mac_port_map_t mac_port_map;

void init_mac_hash_table()
{
	bzero(&mac_port_map, sizeof(mac_port_map_t));

	pthread_mutexattr_init(&mac_port_map.attr);
	pthread_mutexattr_settype(&mac_port_map.attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mac_port_map.lock, &mac_port_map.attr);

	pthread_create(&mac_port_map.tid, NULL, sweeping_mac_port_thread, NULL);
}

void destory_mac_hash_table()
{
	pthread_mutex_lock(&mac_port_map.lock);
	mac_port_entry_t *tmp, *entry;
	for (int i = 0; i < HASH_8BITS; i++) {
		entry = mac_port_map.hash_table[i];
		if (!entry) 
			continue;

		tmp = entry->next;
		while (tmp) {
			entry->next = tmp->next;
			free(tmp);
			tmp = entry->next;
		}
		free(entry);
	}
	pthread_mutex_unlock(&mac_port_map.lock);
}

iface_info_t *lookup_port(u8 mac[ETH_ALEN])
{	
	u8 hash_key = hash8((unsigned char*)mac, ETH_ALEN);
	log(DEBUG, "the look-up mac address is " ETHER_STRING ".\n", ETHER_FMT(mac));

	pthread_mutex_lock(&mac_port_map.lock);
	mac_port_entry_t* entry = mac_port_map.hash_table[hash_key];
	while(entry) 
	{
		if (bcmp(mac, entry->mac, ETH_ALEN)) 
		{
			entry = entry->next;
		}
		else
		{
			entry->visited = time(NULL);
			break;
		} 
	}

    pthread_mutex_unlock(&mac_port_map.lock);

	return (entry ? entry->iface : NULL);
}

void insert_mac_port(u8 mac[ETH_ALEN], iface_info_t *iface)
{
	mac_port_entry_t* entry_ins;
	entry_ins = (mac_port_entry_t*)malloc(sizeof(mac_port_entry_t));
	log(DEBUG, "the inserting mac address is " ETHER_STRING ".\n", ETHER_FMT(mac));
    pthread_mutex_lock(&mac_port_map.lock);

    bcopy(mac, entry_ins->mac, ETH_ALEN);
	entry_ins->iface = iface;
	entry_ins->visited = time(NULL);
	u8 hash_key = hash8((unsigned char*)mac, ETH_ALEN);
	entry_ins->next = mac_port_map.hash_table[hash_key];
	mac_port_map.hash_table[hash_key] = entry_ins;

    pthread_mutex_unlock(&mac_port_map.lock);
}

void dump_mac_port_table()
{
	mac_port_entry_t *entry = NULL;
	time_t now = time(NULL);

	fprintf(stdout, "dumping the mac_port table:\n");
	pthread_mutex_lock(&mac_port_map.lock);
	for (int i = 0; i < HASH_8BITS; i++) {
		entry = mac_port_map.hash_table[i];
		while (entry) {
			fprintf(stdout, ETHER_STRING " -> %s, %d\n", ETHER_FMT(entry->mac), \
					entry->iface->name, (int)(now - entry->visited));

			entry = entry->next;
		}
	}

	pthread_mutex_unlock(&mac_port_map.lock);
}

int sweep_aged_mac_port_entry()
{
	int swp_count = 0;
	mac_port_entry_t* pre;
	mac_port_entry_t* cur;
	mac_port_entry_t* tmp;
	pthread_mutex_lock(&mac_port_map.lock);

	for(int i = 0; i < HASH_8BITS; ++i)
	{
		pre = mac_port_map.hash_table[i];
		if(pre) cur = pre;
		else continue;
		if(pre->next) tmp = pre->next;
		else continue;

		while(time(NULL) >= MAC_PORT_TIMEOUT + cur->visited)
		{
			free(cur);
			swp_count++;
			pre = tmp;
			cur = tmp;
			tmp = tmp->next;
		}
		mac_port_map.hash_table[i] = pre;
		cur = pre->next;
		if(cur) tmp = cur->next;
		else continue;

		while(cur)
		{
			if(time(NULL) >= MAC_PORT_TIMEOUT + cur->visited)
			{
				free(cur);
				pre->next = tmp;
				cur = tmp;
				tmp = tmp->next;
				swp_count++;
			}
			else
			{
				pre = pre->next;
				cur = cur->next;
				tmp = tmp->next;
			}
		}
	}		

    pthread_mutex_unlock(&mac_port_map.lock);
	return swp_count;
}

void *sweeping_mac_port_thread(void *nil)
{
	while (1) {
		sleep(1);
		int n = sweep_aged_mac_port_entry();

		if (n > 0)
		{
			log(DEBUG, "%d aged entries in mac_port table are removed.\n", n);		
		}
		else
		{
			log(DEBUG, "NO aged entries in mac_port table now.\n");
		}
	}

	return NULL;
}


