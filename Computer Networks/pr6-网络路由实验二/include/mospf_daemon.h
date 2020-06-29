#ifndef __MOSPF_DAEMON_H__
#define __MOSPF_DAEMON_H__

#include "base.h"
#include "types.h"
#include "list.h"

typedef struct
{
    u32 rid;
    u32 dist;
    iface_info_t *send_iface;
    u32 gw;
    u32 visited;
} graph_t;

void lsu_update();
void mospf_init();


void nbr_update(int index, int rt_count, graph_t graph_node[]);
int set_visited(int rt_count, graph_t graph_node[]);
void initial_graph(int rt_count, graph_t graph_node[]);
void *rtable_update(void *param);

void mospf_init();
void mospf_run();
void handle_mospf_packet(iface_info_t *iface, char *packet, int len);

#endif

