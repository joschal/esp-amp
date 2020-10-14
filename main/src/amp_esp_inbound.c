#include <string.h>
#include "amp_esp_inbound.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"

#include "mesh_internals.h"
#include "amp_msg_handler.h"
#include "amp_globals.h"

#define LOG_TAG             "ESP_INBOUND"

static uint8_t rx_buf[RX_SIZE] = {0,};

/**
 * This is a task
 * Does asyncronously receive messages via ESP Mesh
 * Puts messages in inbound and routing queue
 */
void amp_esp_inbound(void *arg) {

	mesh_addr_t from;
	mesh_data_t data;
	data.data = rx_buf;
	data.size = RX_SIZE;

	int flag = 0;
	struct msg_envelope msgEnvelope;
	struct header_envelope headerEnvelope;
	union amp_msg *receivedMsg = malloc(AMP_MESSAGE_SIZE);

	while (true) {
		ESP_LOGD(LOG_TAG, "Will wait for message");
		ESP_ERROR_CHECK(esp_mesh_recv(&from, &data, portMAX_DELAY, &flag, NULL, 0));

		memcpy(receivedMsg, data.data, AMP_MESSAGE_SIZE);

		ESP_LOGI(LOG_TAG, "received message of type %X from "
				MACSTR, receivedMsg->generic_msg.header.msg_type, MAC2STR(from.addr));

		msgEnvelope.inbound_interface_addr = from;
		// This should use the size of the union
		memcpy(&msgEnvelope.msg, receivedMsg, AMP_MESSAGE_SIZE);

		xQueueSend(inboundMessages, &msgEnvelope, QUEUE_TIMEOUT);
		ESP_LOGD(LOG_TAG, "Did send message to inbound queue");

		// send header info to routing queue
		headerEnvelope.inbound_interface_addr = from;
		headerEnvelope.msg_type = receivedMsg->generic_msg.header.msg_type;
		headerEnvelope.source_address = receivedMsg->generic_msg.header.source;

		// check if message is forwardable or not
		if (receivedMsg->generic_msg.header.msg_type > 0xD0) {
			// is forwardable
			headerEnvelope.hopCount = ((union amp_msg_forwardable *) receivedMsg)->generic_forwardable_msg.header.hop_count;
		} else {
			// is not forwardable
			headerEnvelope.hopCount = 0;
		}
		xQueueSend(routingInfo, &headerEnvelope, QUEUE_TIMEOUT);
		ESP_LOGD(LOG_TAG, "Did send message to routing queue");

		// Set memory to zero for next cycle
		memset(&msgEnvelope, 0, sizeof(struct msg_envelope));
		memset(&headerEnvelope, 0, sizeof(struct header_envelope));
	}

}
