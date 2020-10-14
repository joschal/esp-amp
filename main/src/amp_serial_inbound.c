#include "esp_log.h"
#include <driver/uart.h>
#include <include/amp_msg_handler.h>
#include <include/amp_addressing.h>
#include "string.h"
#include "driver/gpio.h"
#include "amp_serial_inbound.h"
#include "amp_messages.h"

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)

void serial_init(void) {
	const uart_config_t uart_config = {
			.baud_rate = 115200,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config(UART_NUM_1, &uart_config);
	uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	// We won't use a buffer for sending data.
	uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
	ESP_ERROR_CHECK(uart_set_mode(UART_NUM_1, UART_MODE_RS485_COLLISION_DETECT));
}

void amp_serial_hello(void *arg) {

	static const char *LOG_TAG = "SERIAL_HELLO";

	amp_msg_c_hello *hello = malloc(sizeof(amp_msg_c_hello));
	memset(hello, 0, sizeof(amp_msg_c_hello));
	hello->header.destination = 0;
	hello->header.source = local_amp_address;
	hello->header.msg_type = AMP_MSGT_C_HELLO;

	while (serial_gateway_peer_address == 0) {

		ESP_LOGI(LOG_TAG, "peers address is [%lld]", serial_gateway_peer_address);
		ESP_LOGI(LOG_TAG, "Will send hello message with via serial port, using source address [%lld]",
		         hello->header.source);

		int txBytes = amp_serial_send((union amp_msg *) hello, sizeof(amp_msg_c_hello));

		ESP_LOGI(LOG_TAG, "Did send %d bytes", txBytes);

		vTaskDelay(1000 * 5 / portTICK_RATE_MS);
	}

	free(hello);
	vTaskDelete(amp_serial_hello_task);

}

int amp_serial_send(union amp_msg *msg, int bytecount) {

	static const char *LOG_TAG = "SERIAL_TX";

	// terminate message with ASCII ETO character
	uint8_t eot = 0x04;

	ESP_LOGI(LOG_TAG, "Will send message with %d bytes via serial port", bytecount);
	ESP_LOG_BUFFER_HEXDUMP(LOG_TAG, msg, bytecount, ESP_LOG_INFO);
	// check sign of buffer & endianness
	int txBytes = uart_write_bytes(UART_NUM_1, (void *) msg->buffer, bytecount);
	txBytes += uart_write_bytes(UART_NUM_1, (void *) &eot, 1);

	return txBytes;

}

void amp_serial_inbound(void *arg) {

	static const char *LOG_TAG = "SERIAL_RX";
	uint8_t *data = (uint8_t *) malloc(AMP_MESSAGE_SIZE + 1);
	struct msg_envelope envelope;

	bool serial_receive = true;

	ESP_LOGI(LOG_TAG, "Start to listen for serial messages");

	while (serial_receive) {
		// Clear memory
		bzero(data, AMP_MESSAGE_SIZE + 1);

		// read from serial port
		const int rxBytes = uart_read_bytes(UART_NUM_1, data, AMP_MESSAGE_SIZE, 1000 / portTICK_RATE_MS);
		if (rxBytes > 0) {

			// Terminate string with zero
			data[rxBytes] = 0;

			ESP_LOGI(LOG_TAG, "Read %d bytes: '%s'", rxBytes, data);
			ESP_LOG_BUFFER_HEXDUMP(LOG_TAG, data, rxBytes, ESP_LOG_INFO);

			// Copy data into envelope
			memcpy(&envelope.msg, data, rxBytes);
			// Serial is always address 00:00:00:00:00:00
			bzero(envelope.inbound_interface_addr.addr, sizeof(mesh_addr_t));

			ESP_LOGI(LOG_TAG, "Received message of type %#X from [ %lld ]", envelope.msg.generic_msg.header.msg_type,
			         envelope.msg.generic_msg.header.source);
			xQueueSend(inboundMessages, &envelope, QUEUE_TIMEOUT);
		}
	}
}
