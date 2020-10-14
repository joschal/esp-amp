#ifndef INTERNAL_COMMUNICATION_AMP_FORWARDER_H
#define INTERNAL_COMMUNICATION_AMP_FORWARDER_H

#include "amp_routing.h"

void amp_outbound(void *args);

void send_to_all_mesh_children(mesh_data_t *meshData, mesh_addr_t from_addr);

void flood_amp_msg(union amp_msg *msg, mesh_addr_t from_addr);

extern mesh_addr_t mesh_parent_addr;
extern route_t routing_table[];
extern int mesh_children_count;
extern mesh_addr_t esp_mesh_children[];
#endif //INTERNAL_COMMUNICATION_AMP_FORWARDER_H
