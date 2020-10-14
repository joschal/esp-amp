#ifndef INTERNAL_COMMUNICATION_AMP_GOBALS_H
#define INTERNAL_COMMUNICATION_AMP_GOBALS_H

#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"

#define ROUTING_TABLE_SIZE  (10)

#define QUEUE_TIMEOUT       (5 * 1000  / portTICK_RATE_MS)

address local_amp_address;

address serial_gateway_peer_address;

QueueHandle_t *inboundMessages, *outboundMessages, *toBusinessLogic, *routingInfo;

TaskHandle_t *amp_serial_hello_task, *acquire_address_from_parent_task;


/**
 * This struct is needed to transfer the layer 2 source information between tasks.
 */
struct msg_envelope {
	mesh_addr_t inbound_interface_addr;
	union amp_msg msg;
};

struct header_envelope {
	mesh_addr_t inbound_interface_addr;
	msg_type msg_type;
	address source_address;
	uint8_t hopCount;
};

#endif //INTERNAL_COMMUNICATION_AMP_GOBALS_H
