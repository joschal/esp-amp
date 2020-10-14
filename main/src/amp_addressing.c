#include <string.h>
#include <include/amp_serial_inbound.h>
#include <include/gpio_for_bl.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"

#include "amp_addressing.h"
#include "mesh_internals.h"
#include "amp_messages.h"
#include "amp_routing.h"
#include "amp_msg_handler.h"
#include "amp_globals.h"

#define LOG_TAG "ADDRESSING"

address_pool_with_state_t address_pools[TOTAL_ADDRESS_POOLS] = {0};

bool address_pools_table_locked = false;


/**
 * This is a task.
 * It is triggered when the node starts up and does need to acquire an address from it's parent.
 * Hello messages are send directly ot the parent. A general purpose implementation should be more specific.
 * But since the ESP-AMP implementaiton does always get it's address from root, this is fine.
 */
void acquire_address_from_parent(void *arg) {

	// Since this node has no address and no way of knowing the address of it's parent, both are set to 0
	amp_msg_c_hello hello = {{AMP_MSGT_C_HELLO, 0, 0}};

	// Do this while the node has no address.
	while (local_amp_address == 0) {
		ESP_LOGI("AMP_HELLO", "Will send hello message to parent "
				MACSTR, MAC2STR(mesh_parent_addr.addr));

		mesh_data_t *meshData = malloc(sizeof(mesh_data_t));
		meshData->data = (uint8_t *) &hello;
		meshData->size = AMP_MESSAGE_SIZE;
		meshData->proto = MESH_PROTO_BIN;
		meshData->tos = MESH_TOS_P2P;

		ESP_ERROR_CHECK(esp_mesh_send(
				&mesh_parent_addr, // Pointer to destination address
				meshData, // Pointer to data
				MESH_DATA_P2P, // Flag to indicate peer-to-peer message
				NULL, // options like groups
				0)); // how many options were given

		ESP_LOGI("AMP_HELLO", "Did send hello message to parent");

		// Wait 2 seconds between transmissions
		vTaskDelay(5 * 1000 / portTICK_RATE_MS);
	}

	// When an address is assigned, this task can be deleted
	vTaskDelete(acquire_address_from_parent_task);
}

/**
 * Is executed by root node
 */
void use_root_address_pool(void) {
	if (esp_mesh_is_root() &&
	    local_amp_address == 0) {

		// clear local address
		local_amp_address = 0;

		// clear existing pools
		while (address_pools_table_locked) {}
		address_pools_table_locked = true;
		memset(&address_pools[0], 0, sizeof(address_pools));
		address_pools_table_locked = false;

		// assign new pool
		address_pool_with_state_t pool = {0};
		pool.startAddress = AMP_ESP_ROOT_ADDRESS_POOL_START;
		pool.size = AMP_ESP_ROOT_ADDRESS_POOL_SIZE;
		pool.state = AVAILABLE;
		add_available_address_pool(pool);
	}

}

void assign_address_from_local_pool(void) {

	if (local_amp_address != 0) {
		ESP_LOGE(LOG_TAG, "This node already as an address: %lld", local_amp_address);
		return;
	}

	// wait for lock
	while (address_pools_table_locked) {}
	address_pools_table_locked = true;

	if (address_pools[0].startAddress == 0 ||
	    address_pools[0].size <= 0) {
		ESP_LOGE(LOG_TAG, "There is no address pool available to use an address from");

		// unlock
		address_pools_table_locked = false;
		return;
	}

	// assign lowest address to this node
	local_amp_address = address_pools[0].startAddress;

	// resize pool
	address_pools[0].startAddress++;
	address_pools[0].size--;

	// unlock
	address_pools_table_locked = false;

	ESP_LOGI(LOG_TAG, "Assigned an address to this node from local pool: [%lld]", local_amp_address);

}

void handle_hello(amp_msg_c_hello *msgHello, mesh_addr_t *from) {
	ESP_LOGI("HELLO_HANDLER", "I received a hello message of type %#X to address %lld from %lld",
	         msgHello->header.msg_type,
	         msgHello->header.destination,
	         msgHello->header.source);

	// helper variable to check for empty froma ddr
	mesh_addr_t empty = {{0, 0, 0, 0, 0, 0}};

	if (msgHello->header.source == 0) {


		address_pool_with_state_t reserved_pool = reserve_address_pool(from);

		amp_msg_header header = {AMP_MSGT_A_POOL_ADVERTISEMENT, local_amp_address, 0};
		amp_msg_a_pool_advertisement advertisement = {header, 1, {{reserved_pool.startAddress, reserved_pool.size}}};

		ESP_LOGI("HELLO_HANDLER", "Will send address advertisement [%lld | %lld] to "
				MACSTR, advertisement.pools[0].start_address, advertisement.pools[0].pool_size, MAC2STR(from->addr));

		send_amp_message_via_layer_2((union amp_msg *) &advertisement, from);
	} else if (compareLayer2Addresses(from->addr, empty.addr)) {
		ESP_LOGI("HELLO_HANDLER", "Did receive HELLO from serial peer with AMP address [%lld]",
		         msgHello->header.source);
		memcpy(&serial_gateway_peer_address, &msgHello->header.source, sizeof(address));
	} else {

		ESP_LOGI("HELLO_HANDLER", "Received a hello message from a node which already has an address: [%lld]",
		         msgHello->header.source);
	}
}

/**
 * Is called by the message resolver to process incoming addressing-messages
 * @param msg message to process
 */
void address_pool_management(union amp_msg *msg, mesh_addr_t *from) {

	if (msg->generic_msg.header.msg_type == AMP_MSGT_A_POOL_ADVERTISEMENT) {
		process_pool_advertisement(&msg->pool_advertisement, from);
	} else if (msg->generic_msg.header.msg_type == AMP_MSGT_A_POOL_ACCEPTED) {
		process_pool_accepted(&msg->pool_accepted, from);
	} else if (msg->generic_msg.header.msg_type == AMP_MSGT_A_POOL_ASSIGNED) {
		process_pool_assigned(&msg->pool_assigned, from);
		assign_address_from_local_pool();
	} else {
		ESP_LOGE(LOG_TAG, "Could not process message of type %#X", msg->generic_msg.header.msg_type);
	}
}

/**
 * Child received advertisement from parent
 * @param msg advertising message
 * @param from layer 2 address via which the advertisement was received
 */
void process_pool_advertisement(amp_msg_a_pool_advertisement *msg, mesh_addr_t *from) {

	if (local_amp_address == 0) {

		ESP_LOGI(LOG_TAG, "Received address advertisement [ %lld | %lld ]. Will send accepted message to parent",
		         msg->pools[0].start_address, msg->pools[0].pool_size);

		amp_msg_a_pool_accepted accepted = {{AMP_MSGT_A_POOL_ACCEPTED, 0, msg->header.source}};

		send_amp_message_via_layer_2((union amp_msg *) &accepted, from);

	} else {
		ESP_LOGE(LOG_TAG,
		         "Received advertising message, although there is already a local address. Will drop the message");
		return;
	}
}

void process_pool_accepted(amp_msg_a_pool_accepted *msg, mesh_addr_t *from) {

	address_pool_with_state_t assignedPool = assign_address_pool(from);

	if (assignedPool.size != 0) {

		ESP_LOGI(LOG_TAG, "Will send assigned message to child");

		amp_msg_a_pool_assigned assigned = {{AMP_MSGT_A_POOL_ASSIGNED, 0, 0,}, 1,
		                                    {{assignedPool.startAddress, assignedPool.size}}};


		send_amp_message_via_layer_2((union amp_msg *) &assigned, from);
	} else {
		ESP_LOGE(LOG_TAG,
		         "Could not assign pool. Will drop the message");
		return;
	}

}

void process_pool_assigned(amp_msg_a_pool_assigned *msg, mesh_addr_t *from) {

	if (local_amp_address == 0) {

		ESP_LOGI(LOG_TAG, "Will assign new pool to local list");
		address_pool_with_state_t pool = {0};
		pool.startAddress = msg->pools[0].start_address;
		pool.size = msg->pools[0].pool_size;
		pool.state = AVAILABLE;

		add_available_address_pool(pool);


		ESP_LOGI(LOG_TAG, "Did assign new pool to local list");
	} else {
		ESP_LOGE(LOG_TAG,
		         "Received pool assigned message, although there is already a local address. Will drop the message");
		return;
	}

}

void add_available_address_pool(address_pool_with_state_t address_pool) {

	// wait for lock
	while (address_pools_table_locked) {}
	address_pools_table_locked = true;

	for (int i = 0; i < TOTAL_ADDRESS_POOLS; i++) {
		if (address_pools[i].startAddress == 0) {
			address_pools[i] = address_pool;
			ESP_LOGI(LOG_TAG, "Added new address pool [ %lld | %lld ]to the list of available pools at position [%d]",
			         address_pool.startAddress, address_pool.size, i);

			// unlock
			address_pools_table_locked = false;
			return;
		}
	}

	// unlock
	address_pools_table_locked = false;

	ESP_LOGE(LOG_TAG, "Did not add available address pool");

}

/**
 * TODO implement properly
 * @return
 */
address_pool_with_state_t reserve_address_pool(mesh_addr_t *from) {

	address_pool_with_state_t largest_pool = {0};
	int largest_pool_index = -1;

	// wait for lock
	while (address_pools_table_locked) {}
	address_pools_table_locked = true;

	// If thre already is a reserved pool, use it
	for (int i = 0; i < TOTAL_ADDRESS_POOLS; i++) {
		if (compareLayer2Addresses(address_pools[i].layer_2_address.addr, from->addr)
		    && (address_pools[i].state == RESERVED)) {
			return address_pools[i];
		}
	}

	for (int i = 0; i < TOTAL_ADDRESS_POOLS; i++) {
		// select the largest pool
		if (address_pools[i].size > largest_pool.size &&
		    address_pools[i].state == AVAILABLE) {
			largest_pool = address_pools[i];
			largest_pool_index = i;
			break;
		}
	}

	address largest_address = largest_pool.startAddress + largest_pool.size;
	uint64_t middle = largest_pool.startAddress + (largest_pool.size / 2);

	address_pool_with_state_t free_pool, reserved_pool = {0};

	free_pool.startAddress = largest_pool.startAddress;
	free_pool.size = middle - largest_pool.startAddress;
	free_pool.state = AVAILABLE;

	reserved_pool.startAddress = free_pool.startAddress + free_pool.size + 1;
	reserved_pool.size = largest_address - reserved_pool.startAddress - 1;
	reserved_pool.state = RESERVED;
	memcpy(&reserved_pool.layer_2_address.addr, &from->addr, sizeof(mesh_addr_t));

	address_pools[largest_pool_index] = free_pool;

	for (int i = 0; i < TOTAL_ADDRESS_POOLS; i++) {
		if (address_pools[i].startAddress == 0) {
			address_pools[i] = reserved_pool;
			break;
		}
	}

	//unlock
	address_pools_table_locked = false;

	ESP_LOGI(LOG_TAG, "Reserved pool : [%lld | %lld] free pool: [%lld | %lld]", reserved_pool.startAddress,
	         reserved_pool.size, free_pool.startAddress, free_pool.size);

	return reserved_pool;
}

address_pool_with_state_t assign_address_pool(mesh_addr_t *from) {

	// wait for lock
	while (address_pools_table_locked) {}
	address_pools_table_locked = true;

	ESP_LOGI(LOG_TAG, "Will try to assign address pool to " MACSTR, MAC2STR(from->addr));

	for (int i = 0; i < TOTAL_ADDRESS_POOLS; i++) {

		ESP_LOGI(LOG_TAG, " Comparing to pool [ %lld | %lld ] associated with "MACSTR" state -> %d",
		         address_pools[i].startAddress,
		         address_pools[i].size,
		         MAC2STR(address_pools[i].layer_2_address.addr),
		         address_pools[i].state);

		if (compareLayer2Addresses(address_pools[i].layer_2_address.addr, from->addr) &&
		    address_pools[i].state == RESERVED) {

			address_pools[i].layer_2_address = *from;
			address_pools[i].state = ASSIGNED;

			ESP_LOGI(LOG_TAG, "Set state of address pool [ %lld | %lld ]to ASSIGNED ", address_pools[i].startAddress,
			         address_pools[i].size);

			//unlock
			address_pools_table_locked = false;
			return address_pools[i];
		}
	}

	address_pools_table_locked = false;
	ESP_LOGE(LOG_TAG, "Something went wrong while assigning an address pool");
	address_pool_with_state_t emptypool = {0};
	return emptypool;
}

/**
 * Sets the state of the address pool of the departed neighbor to AVAILABLE
 * TODO defragment address pools like in the simulation
 * @param event ESP-MESH provided event with neighbors MAC
 */
void revoke_address_pool(mesh_addr_t *layer2addr) {
	// wait for lock
	while (address_pools_table_locked) {}
	address_pools_table_locked = true;

	for (int i = 0; i < TOTAL_ADDRESS_POOLS; i++) {

		if (compareLayer2Addresses(address_pools[i].layer_2_address.addr, layer2addr->addr)) {

			address_pools[i].state = AVAILABLE;

			address_pools_table_locked = false;
			ESP_LOGI("ADDRESSING_POOL_REVOKED", "Set state of the address pool [ %lld | %lld ] formerly used by "
					MACSTR
					" to available",
			         address_pools[i].startAddress, address_pools[i].size, MAC2STR(layer2addr->addr));
			return;
		}

	}

	//unlock
	address_pools_table_locked = false;

	ESP_LOGE(LOG_TAG, "Something went wrong while revoking an address pool used by "
			MACSTR, MAC2STR(layer2addr->addr));
}

/**
 * Returns appropriate address
 * @param msg
 * @return
 */
mesh_addr_t *get_layer_2_address(union amp_msg *msg) {
	address amp_destination = msg->generic_msg.header.destination;
	mesh_addr_t *layer_2_addr = NULL;

	// wait until routing table is unlocked
	while (routing_table_locked) {}
	routing_table_locked = true;

	ESP_LOGI("LAYER_2_ADDRESS", "Will retreive address for %lld ", msg->generic_msg.header.destination);

	for (int i = 0; i < ROUTING_TABLE_SIZE; i++) {

		if (routing_table[i].address == amp_destination) {
			layer_2_addr = &routing_table[i].layer_2_address;

			ESP_LOGI("LAYER_2_ADDRESS", "Did retrieve address for %lld -> "
					MACSTR, msg->generic_msg.header.destination, MAC2STR(layer_2_addr->addr));
			break;
		}
	}

	// unlock routing table
	routing_table_locked = false;

	return layer_2_addr;
}


