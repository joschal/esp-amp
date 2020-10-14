#include <string.h>
#include "esp_event.h"
#include "esp_log.h"

#include "amp_routing.h"
#include "amp_msg_handler.h"
#include "amp_addressing.h"

#define LOG_TAG     "ROUTER"

static struct header_envelope envelope_buf;

int mesh_children_count = 0;

mesh_addr_t esp_mesh_children[CONFIG_MESH_AP_CONNECTIONS];
/**
 * Compares if two esp-mesh addresses are equal
 * @param a mesh_addr_t
 * @param b mesh_addr_t
 * @return boolean
 */
bool compareLayer2Addresses(uint8_t a[], uint8_t b[]) {
	int i;
	for (i = 0; i < 6; i++) {
		if (a[i] != b[i])
			return false;
	}
	return true;
}

/**
 * This task peaks at the queue of received messages and updates the routing table accordingly
 * @param args
 */

void update_routing_table(void *args) {

	bool receive = true;
	struct header_envelope *headerEnvelope;

	while (receive) {
		ESP_LOGD(LOG_TAG, "Waiting for Queue to be populated");
		// receive inbound messages from queue
		if (xQueueReceive(routingInfo, &envelope_buf, portMAX_DELAY)) {

			ESP_LOGI(LOG_TAG, "decoded envelope from "
					MACSTR
					" with type %#X", MAC2STR(envelope_buf.inbound_interface_addr.addr),
			         envelope_buf.msg_type);

			headerEnvelope = &envelope_buf;

			// Filter out addresses which shall not to be stored in the routing table
			if (headerEnvelope->source_address == 0 ||
			    headerEnvelope->source_address == local_amp_address ||
			    (headerEnvelope->msg_type > 0xD0 && headerEnvelope->hopCount > 0)) {
				ESP_LOGI(LOG_TAG, "Dropped message of type [%#X], since it should not be routed [ %lld ]",
				         headerEnvelope->msg_type, headerEnvelope->source_address);
				continue;
			}

			// wait until routing table is unlocked
			while (routing_table_locked) {}

			// lock routing table to safely work on it
			routing_table_locked = true;

			bool route_found = false;

			// Check if route exists in routing table
			for (int i = 0; i < ROUTING_TABLE_SIZE; i++) {

				ESP_LOGD(LOG_TAG, "%d RoutingTable address: [%lld], received address [%lld]", i,
				         routing_table[i].address, headerEnvelope->source_address);

				// check if the node is already in the routing table (could probably be optimized)
				if (routing_table[i].address == headerEnvelope->source_address) {

					// check if a faster route was found
					if (headerEnvelope->hopCount < routing_table[i].hops) {
						// Update routing table with new, better route
						// Set new TTL
						routing_table[i].hops = headerEnvelope->hopCount;
						routing_table[i].expiration_time = xTaskGetTickCount() + TTL_ROUTE_IN_TICKS;
						memcpy(&routing_table[i].layer_2_address, &headerEnvelope->inbound_interface_addr,
						       sizeof(mesh_addr_t));
						ESP_LOGI(LOG_TAG, "Better route found for %lld with %d hops via "
								MACSTR, headerEnvelope->source_address, headerEnvelope->hopCount,
						         MAC2STR(headerEnvelope->inbound_interface_addr.addr));

						route_found = true;
						break;

					} else if (headerEnvelope->hopCount == routing_table[i].hops &&
					           compareLayer2Addresses(headerEnvelope->inbound_interface_addr.addr,
					                                  routing_table[i].layer_2_address.addr)) {

						// If a route is the same as before, update the TTL, since the route is still valid
						ESP_LOGI(LOG_TAG, "Updated TTL for route to %lld with %d hops via "
								MACSTR, headerEnvelope->source_address, headerEnvelope->hopCount,
						         MAC2STR(headerEnvelope->inbound_interface_addr.addr));
						routing_table[i].expiration_time = xTaskGetTickCount() + TTL_ROUTE_IN_TICKS;

						route_found = true;
						break;
					}
				}
			}

			// if route was not found in the routing table, add it (if there is an empty space in the table)
			if (!route_found) {
				ESP_LOGI(LOG_TAG, "new route should be stored here");

				bool route_stored = false;
				for (int i = 0; i < ROUTING_TABLE_SIZE; i++) {
					if (routing_table[i].address == 0) {

						routing_table[i].address = headerEnvelope->source_address;
						routing_table[i].hops = headerEnvelope->hopCount;
						memcpy(&routing_table[i].layer_2_address, &headerEnvelope->inbound_interface_addr.addr,
						       sizeof(mesh_addr_t));
						routing_table[i].expiration_time = xTaskGetTickCount() + TTL_ROUTE_IN_TICKS;
						route_stored = true;
						ESP_LOGI(LOG_TAG, "Added route to routing table at empty space [%d]", i);
						break;
					} else if (routing_table[i].expiration_time < xTaskGetTickCount()) {
						routing_table[i].address = headerEnvelope->source_address;
						routing_table[i].hops = headerEnvelope->hopCount;
						memcpy(&routing_table[i].layer_2_address, &headerEnvelope->inbound_interface_addr.addr,
						       sizeof(mesh_addr_t));
						routing_table[i].expiration_time = xTaskGetTickCount() + TTL_ROUTE_IN_TICKS;
						route_stored = true;
						ESP_LOGI(LOG_TAG, "Overwriting timed out route [%d]", i);
						break;
					}
				}

				if (!route_stored) {
					ESP_LOGE(LOG_TAG, "Routing Table is full. Route could not be stored. It will be dropped");
				}
			}

			// unlock routing table when done using it
			routing_table_locked = false;

			// Set memory to zero for next cycle
			memset(&envelope_buf, 0, sizeof(struct header_envelope));
		}
	}
}

void neighbour_connected(mesh_addr_t *addr) {

	ESP_LOGI("ROUTING_NEIGHBOR_CONNECTED", "Will add. There are %d neighbors", mesh_children_count);
	for (int i = 0; i <= mesh_children_count; i++) {

		mesh_addr_t empty = {{0, 0, 0, 0, 0, 0}};

		if (compareLayer2Addresses(esp_mesh_children[i].addr, empty.addr)) {
			memcpy(esp_mesh_children[i].addr, addr->addr, sizeof(mesh_addr_t));
			mesh_children_count++;
			ESP_LOGI("ROUTING_NEIGHBOR_CONNECTED", "Registered neighbor with address "
					MACSTR, MAC2STR(esp_mesh_children[i].addr));
			break;
		}

	}

	ESP_LOGI("ROUTING_NEIGHBOR_CONNECTED", "Did add. There are %d neighbors", mesh_children_count);
}

void neighbour_disconnected(mesh_addr_t *addr) {

	ESP_LOGI("ROUTING_NEIGHBOR_DISCONNECTED", "Will remove. There are %d neighbors", mesh_children_count);
	for (int i = 0; i <= mesh_children_count; i++) {

		if (compareLayer2Addresses(esp_mesh_children[i].addr, addr->addr)) {
			memset(esp_mesh_children[i].addr, 0, sizeof(mesh_addr_t));
			mesh_children_count--;
			ESP_LOGI("ROUTING_NEIGHBOR_DISCONNECTED", "Unregistered neighbor with address "
					MACSTR, MAC2STR(addr->addr));
			break;
		}

	}

	ESP_LOGI("ROUTING_NEIGHBOR_DISCONNECTED", "Did remove. There are %d neighbors", mesh_children_count);
	// wait for unlock
	while (routing_table_locked) {}
	routing_table_locked = true;

	for (int i = 0; i < ROUTING_TABLE_SIZE; i++) {

		if (compareLayer2Addresses(routing_table[i].layer_2_address.addr, addr->addr)) {
			memset(&routing_table[i], 0, sizeof(route_t));
			ESP_LOGI("ROUTING_NEIGHBOR_DISCONNECTED", "Removed route to [ %lld ], since it was connected via "
					MACSTR, routing_table[i].address, MAC2STR(addr->addr));
		}
	}

	//unlock
	routing_table_locked = false;

}
