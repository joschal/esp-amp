#include <string.h>
#include <include/amp_addressing.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"

#include "mesh_internals.h"

#include "amp_globals.h"
#include "amp_msg_handler.h"
#include "amp_routing.h"
#include "amp_outbound.h"
#include "amp_serial_inbound.h"
#include "amp_esp_inbound.h"
#include "business_logic.h"
#include "gpio_for_bl.h"

#define QUEUE_LENGTH        (5)
/*******************************************************
 *                Constants
 *******************************************************/
static const char *MESH_TAG = "mesh_main";


void app_main(void) {

	// Init queues
	inboundMessages = xQueueCreate(QUEUE_LENGTH, sizeof(struct msg_envelope));
	outboundMessages = xQueueCreate(QUEUE_LENGTH, sizeof(struct msg_envelope));
	toBusinessLogic = xQueueCreate(QUEUE_LENGTH, sizeof(struct msg_envelope));
	routingInfo = xQueueCreate(QUEUE_LENGTH, sizeof(struct header_envelope));

	// Init Mesh
	ESP_LOGI(MESH_TAG, "App start");
	serial_init();
	gpio_init();
	init_mesh();
	ESP_LOGI(MESH_TAG, "Mesh initialized");

}

void root_DEBUG_sender(void *arg) {

	bool debugsend = true;

	while (debugsend) {


		amp_msg_d_datagram datagram = {0};
		datagram.header.source = local_amp_address;

		if (esp_mesh_is_root()) {
			ESP_LOGI("DEBUG_SEND", "Will send to 770");
			datagram.header.destination = 770;
			const char *payload = "Melusine";
			memcpy(datagram.payload, payload, strlen(payload));
			datagram.payload_length = strlen(payload);
		} else {
			ESP_LOGI("DEBUG_SEND", "Will send to 1");
			datagram.header.destination = 1;
			const char *payload = "Krawehl";
			memcpy(datagram.payload, payload, strlen(payload));
			datagram.payload_length = strlen(payload);
		}

		datagram.header.hop_count = 0;
		datagram.header.msg_type = AMP_MSGT_D_DATAGRAM;
		datagram.header.hop_limit = 5;

		send_amp_message((union amp_msg *) &datagram);

		// do every 10? seconds
		vTaskDelay(1000 * 10 / portTICK_RATE_MS);

	}

}

esp_err_t mesh_connection_init(mesh_addr_t parent) {

	// set value to zero
	local_amp_address = 0;

	// The first bootstrapping step is to assign an addres to the node.
	if (esp_mesh_is_root()) {
		// if the node is root, it has access to the domains complete address pool and can therefore assign an address to itself
		use_root_address_pool();
		assign_address_from_local_pool();
	} else {
		// If the node is not root, it needs to acquire an address from it's parent. This is done with retry by this task

		xTaskCreate(acquire_address_from_parent, "acquire_address_from_parent", 3072, NULL, 5,
		            acquire_address_from_parent_task);
	}

	// Receive inbound messages
	xTaskCreate(amp_esp_inbound, "amp_esp_inbound", 3072, NULL, 5, NULL);

	// This task looks at every received message and updates the routing table
	xTaskCreate(update_routing_table, "update_routing_table", 3072, NULL, 5, NULL);

	// message resolver/handler
	xTaskCreate(amp_msg_handler, "amp_msg_handler", 3072, NULL, 5, NULL);

	// Business Logic
	xTaskCreate(business_logic, "business_logic", 3072, NULL, 5, NULL);

	// Send outbound messages
	xTaskCreate(amp_outbound, "amp_outbound", 3072, NULL, 5, NULL);

	// DEBUG send periodic messages to child/parent
	int layer = esp_mesh_get_layer(); // root is layer 1
	if (layer == 3) {
		ESP_LOGI("SETUP", "My ESP Mesh layer is %d. Will start debug task", layer);
		xTaskCreate(root_DEBUG_sender, "root_DEBUG_sender", 3072, NULL, 5, NULL);
	}


	// Checks if the node is physically configured as gateway
	if (isNodeGateway()) {
		ESP_LOGI("SETUP", "Node is configured as gateway");
		xTaskCreate(amp_serial_hello, "amp_serial_hello", 3072, NULL, 5, amp_serial_hello_task);
		xTaskCreate(amp_serial_inbound, "amp_serial_inbound", 3072, NULL, 5, NULL);
	} else {
		ESP_LOGI("SETUP", "Node node is not a gateway");
	}
	return ESP_OK;
}