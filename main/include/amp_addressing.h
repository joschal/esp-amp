#ifndef INTERNAL_COMMUNICATION_AMP_ADDRESSING_H
#define INTERNAL_COMMUNICATION_AMP_ADDRESSING_H

#include <sdkconfig.h>
#include <esp_mesh.h>
#include "amp_messages.h"

#define AMP_ESP_ROOT_ADDRESS_POOL_START     (1)
#define AMP_ESP_ROOT_ADDRESS_POOL_SIZE      (1024)
#define TOTAL_ADDRESS_POOLS (64)

enum address_state {
	AVAILABLE, RESERVED, ASSIGNED
};

typedef struct {
	address startAddress;
	uint64_t size; // needs to be 64 bit to theoretically cover the complete address pool;
	mesh_addr_t layer_2_address; // indicates, via which network interface the pool is assigned
	enum address_state state;
} address_pool_with_state_t;

// Holds addresses wor the maximum number of childs, which is defined via the sdk-config
extern address_pool_with_state_t address_pools[];


/**
 * Task to send hello message to parent
 */
void acquire_address_from_parent(void *arg);

void assign_address_from_local_pool(void);

void handle_hello(amp_msg_c_hello *msgHello, mesh_addr_t *from);

void address_pool_management(union amp_msg *msg, mesh_addr_t *from);

void process_pool_advertisement(amp_msg_a_pool_advertisement *msg, mesh_addr_t *from);

void process_pool_accepted(amp_msg_a_pool_accepted *msg, mesh_addr_t *from);

void process_pool_assigned(amp_msg_a_pool_assigned *msg, mesh_addr_t *from);

mesh_addr_t *get_layer_2_address(union amp_msg *msg);

/**
 * If a new child node reqests an address pool, set it's state to RESERVED
 * @return address pool, the child node may choose to use
 */
address_pool_with_state_t reserve_address_pool(mesh_addr_t *from);


/**
 * If a child node accepts an address pool advertisement, set it's state to ASSIGNED
 */
address_pool_with_state_t assign_address_pool(mesh_addr_t *from);

/**
 * If a child node goes offline or declines a pool advertisement, set it's state to AVAILABLE
 * @param address_pool Pool to revoke
 */
void revoke_address_pool(mesh_addr_t *layer2addr);

/**
 * If a new address pool becomes available, store it, with the state AVAILABLE
 * @param address_pool Pool to add
 */
void add_available_address_pool(address_pool_with_state_t address_pool);

void use_root_address_pool(void);

void root_DEBUG_sender(void *arg);

#endif //INTERNAL_COMMUNICATION_AMP_ADDRESSING_H
