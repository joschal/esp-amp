#ifndef INTERNAL_COMMUNICATION_AMP_ROUTING_H
#define INTERNAL_COMMUNICATION_AMP_ROUTING_H

#include <esp_mesh.h>
#include "amp_messages.h"
#include "amp_globals.h"

#define TTL_ROUTE_IN_TICKS      (1000)

void update_routing_table(void *args);

bool compareLayer2Addresses(uint8_t a[], uint8_t b[]);

void neighbour_connected(mesh_addr_t *addr);

void neighbour_disconnected(mesh_addr_t *addr);

typedef struct {
	address address;
	mesh_addr_t layer_2_address;
	uint8_t hops;
	TickType_t expiration_time;
} route_t;

static bool routing_table_locked = false;

// TODO should this be "extern" variables?
static route_t routing_table[ROUTING_TABLE_SIZE];


#endif //INTERNAL_COMMUNICATION_AMP_ROUTING_H
