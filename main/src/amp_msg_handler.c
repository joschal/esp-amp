#include <string.h>
#include <include/amp_routing.h>
#include "esp_event.h"
#include "esp_log.h"

#include "mesh_internals.h"
#include "amp_msg_handler.h"
#include "amp_globals.h"
#include "amp_addressing.h"


#define LOG_TAG             "RESOLVER"

static struct msg_envelope envelope_buf;

void amp_msg_handler(void *arg) {

	bool receive = true;
	union amp_msg *receivedMsg;

	while (receive) {
		// receive inbound messages from queue. It does not matter
		if (xQueueReceive(inboundMessages, &envelope_buf, portMAX_DELAY)) {

			ESP_LOGI("RESOLVER", "decoded envelope from ["
					MACSTR
			" | %lld ]"
			" with type %x", MAC2STR(envelope_buf.inbound_interface_addr.addr),
					envelope_buf.msg.generic_msg.header.source,
			         envelope_buf.msg.generic_msg.header.msg_type);

			receivedMsg = &envelope_buf.msg;

			if (receivedMsg->generic_msg.header.msg_type == AMP_MSGT_C_HELLO) {
				handle_hello((amp_msg_c_hello *) receivedMsg, &envelope_buf.inbound_interface_addr);

			} else if (receivedMsg->generic_msg.header.msg_type == AMP_MSGT_C_GOODBYE) {

				neighbour_disconnected(&envelope_buf.inbound_interface_addr);
				revoke_address_pool(&envelope_buf.inbound_interface_addr);

				amp_msg_c_bye_ack bye_ack;
				bye_ack.header.destination = receivedMsg->generic_msg.header.source;
				bye_ack.header.source = receivedMsg->generic_msg.header.destination;
				bye_ack.header.msg_type = AMP_MSGT_C_GOODBYE_ACK;

				ESP_LOGI(LOG_TAG, "Will send goodbye ack message to "
				MACSTR, MAC2STR(envelope_buf.inbound_interface_addr.addr));
				send_amp_message_via_layer_2((union amp_msg *) &bye_ack, &envelope_buf.inbound_interface_addr);

			} else if (
					receivedMsg->generic_msg.header.msg_type == AMP_MSGT_D_DATAGRAM ||
					receivedMsg->generic_msg.header.msg_type == AMP_MSGT_D_ACKNOWLEDGED_DATAGRAM ||
					receivedMsg->generic_msg.header.msg_type == AMP_MSGT_D_DATAGRAM_ACK) {

				if (receivedMsg->generic_msg.header.destination == local_amp_address) {
					ESP_LOGI(LOG_TAG, "This message from %lld is addressed to this node and needs to be processed",
					         receivedMsg->generic_msg.header.source);

					xQueueSend(toBusinessLogic, &envelope_buf, QUEUE_TIMEOUT);
				} else {

					// This message types need to be forwarded

					ESP_LOGI("TO_FORWARDER", "Will send message to outbound queue");
					xQueueSend(outboundMessages, &envelope_buf, QUEUE_TIMEOUT);
				}

			} else if (receivedMsg->generic_msg.header.msg_type == AMP_MSGT_A_POOL_ADVERTISEMENT ||
					   receivedMsg->generic_msg.header.msg_type == AMP_MSGT_A_POOL_ACCEPTED ||
					   receivedMsg->generic_msg.header.msg_type == AMP_MSGT_A_POOL_ASSIGNED) {
				address_pool_management(receivedMsg, &envelope_buf.inbound_interface_addr);

			} else if (receivedMsg->generic_msg.header.msg_type == AMP_MSGT_A_BIN_CAPACITY_REQ ||
					   receivedMsg->generic_msg.header.msg_type == AMP_MSGT_A_BIN_CAPACITY_REPL ||
					   receivedMsg->generic_msg.header.msg_type == AMP_MSGT_C_GOODBYE_ACK) {

				ESP_LOGE(LOG_TAG, "Message handling for type %x not implemented",
				         receivedMsg->generic_msg.header.msg_type);
			} else if (receivedMsg->generic_msg.header.msg_type == AMP_MSGT_R_ROUTE_DISCOVERY) {

				amp_msg_r_route_reply reply_msg;
				reply_msg.header.hop_count = 0;
				reply_msg.header.source = local_amp_address;
				reply_msg.header.destination = receivedMsg->generic_msg.header.source;
				//reply_msg.header.ttl = receivedMsg->generic_msg.header.hops;

			} else {
				ESP_LOGE("RESOLVER", "Could not handle message of type %x",
				         receivedMsg->generic_msg.header.msg_type);
			}

			// Set memory to zero for next cycle
			memset(&envelope_buf, 0, sizeof(struct msg_envelope));
		}
	}
}
