#include <include/amp_msg_handler.h>
#include "business_logic.h"
#include "esp_event.h"
#include "esp_log.h"

#include "amp_globals.h"
#include "gpio_for_bl.h"

#define LOG_TAG "BUSINESS_LOGIC"

static struct msg_envelope envelope_buf;

void business_logic(void *args) {

	bool to_business_logic = true;
	union amp_msg *receivedMsg;


	while (to_business_logic) {

		if (xQueueReceive(toBusinessLogic, &envelope_buf, portMAX_DELAY)) {

			toggleOutput();
			receivedMsg = &envelope_buf.msg;
			ESP_LOGI(LOG_TAG, "Received datagram of type %#X from %lld with length %i",
			         receivedMsg->datagram.header.msg_type,
			         receivedMsg->datagram.header.source,
			         receivedMsg->datagram.payload_length);

			const char *payload = malloc(receivedMsg->datagram.payload_length);
			memcpy(payload, receivedMsg->datagram.payload, receivedMsg->datagram.payload_length);

			ESP_LOGI(LOG_TAG, "Received payload %.*s of length %i",
			         receivedMsg->datagram.payload_length, payload,
			         receivedMsg->datagram.payload_length);
		}
	}

}
