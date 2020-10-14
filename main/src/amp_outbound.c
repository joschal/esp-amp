#include <stdbool.h>
#include <include/amp_serial_inbound.h>
#include <include/amp_addressing.h>
#include <include/gpio_for_bl.h>
#include "amp_msg_handler.h"
#include "amp_globals.h"
#include "amp_outbound.h"
#include "amp_messages.h"
#include "esp_event.h"
#include "esp_log.h"
#include "amp_routing.h"

#define LOG_TAG "FORWARDER"

static uint8_t queueBuf[sizeof(struct msg_envelope)] = {0};

// TODO broadcast if destination is unknown
void amp_outbound(void *args) {

	bool forward = true;

	while (forward) {

		if (xQueueReceive(outboundMessages, &queueBuf, portMAX_DELAY)) {

			struct msg_envelope *envelope = (struct msg_envelope *) queueBuf;
			union amp_msg_forwardable *msg = (union amp_msg_forwardable *) envelope->msg.buffer;
			mesh_addr_t *destAddr = get_layer_2_address((union amp_msg *) msg);

			if (msg->generic_forwardable_msg.header.hop_count >= msg->generic_forwardable_msg.header.hop_limit) {
				ESP_LOGE(LOG_TAG, "Message to [ %lld ] dropped. TTL was reached",
				         msg->generic_forwardable_msg.header.destination);
				continue;
			}

			// Inclement hop counter before forwarding;
			msg->generic_forwardable_msg.header.hop_count++;

			mesh_data_t *meshData = malloc(sizeof(mesh_data_t));
			meshData->data = msg->buffer;
			meshData->size = AMP_MESSAGE_SIZE;
			meshData->proto = MESH_PROTO_BIN;
			meshData->tos = MESH_TOS_P2P;

			if (destAddr != NULL) {


				ESP_LOGI(LOG_TAG, "Found route for message. Will send it to"
						MACSTR, MAC2STR(destAddr->addr));
				ESP_ERROR_CHECK(esp_mesh_send(destAddr, meshData, MESH_DATA_P2P, NULL, 0));

			} else {

				ESP_LOGI(LOG_TAG, "Did not find a route. Will need to flood");

				flood_amp_msg((union amp_msg *) msg, envelope->inbound_interface_addr);
			}

		}

	}

}

void flood_amp_msg(union amp_msg *msg, mesh_addr_t from_addr) {

	mesh_data_t *meshData = malloc(sizeof(mesh_data_t));
	meshData->data = msg->buffer;
	meshData->size = AMP_MESSAGE_SIZE;
	meshData->proto = MESH_PROTO_BIN;
	meshData->tos = MESH_TOS_P2P;

	// send via serial port
	mesh_addr_t empty = {{0, 0, 0, 0, 0, 0}};
	if (compareLayer2Addresses(from_addr.addr, empty.addr)) {
		ESP_LOGI(LOG_TAG, "Did receive this message from serial prt. Will not flood via serial");
	} else if (isNodeGateway()) {
		amp_serial_send((union amp_msg *) msg, AMP_MESSAGE_SIZE);
		ESP_LOGI(LOG_TAG, "Did not find a route. Did flood via serial");
	}

	// This workaround compensates for a bug in the ESP mesh framework where source address of parent message is off by one
	mesh_addr_t parentCompensator = {0};
	memcpy(&parentCompensator, from_addr.addr, sizeof(mesh_addr_t));
	parentCompensator.addr[5]++;

	if (compareLayer2Addresses(mesh_parent_addr.addr, from_addr.addr) ||
	    compareLayer2Addresses(mesh_parent_addr.addr, parentCompensator.addr)) {
		ESP_LOGI(LOG_TAG, "Did receive this message from parent. Will not flood to parent");
	} else if (!esp_mesh_is_root()) {
		// send upwards in mesh tree. Root node does not do this
		ESP_ERROR_CHECK(esp_mesh_send(&mesh_parent_addr, meshData, MESH_DATA_P2P, NULL, 0));
		ESP_LOGI(LOG_TAG, "Did not find a route. Did flood to parent");
	}

	// send downwards in mesh tree
	send_to_all_mesh_children(meshData, from_addr);
	ESP_LOGI(LOG_TAG, "Did not find a route. Did flood to all child neighbours ");
}


void send_to_all_mesh_children(mesh_data_t *meshData, mesh_addr_t from_addr) {

	ESP_LOGI("FLOODER", "Will send messages to %d children", mesh_children_count);

	for (int i = 0; i < mesh_children_count; i++) {

		if (compareLayer2Addresses(esp_mesh_children[i].addr, from_addr.addr)) {
			ESP_LOGI(LOG_TAG, "Omiting flooding this child");
		} else {
			ESP_LOGI(LOG_TAG, "Will flood to " MACSTR, MAC2STR(esp_mesh_children[i].addr));
			ESP_ERROR_CHECK(
					esp_mesh_send((mesh_addr_t *) &esp_mesh_children[i].addr, meshData, MESH_DATA_P2P, NULL, 0));
		}
	}
}
