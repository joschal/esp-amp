#include <esp_log.h>
#include "stdint.h"
#include "driver/gpio.h"

#include "gpio_for_bl.h"

void toggleOutput() {

	static uint16_t counter = 1;
	gpio_set_level(GPIO_OUTPUT_IO_0, counter % 2);
	counter++;

}

/**
 * If pins 12 & 13 are shorted, the node assumes, it is a gateway
 * @return boolean
 */
bool isNodeGateway() {

	return gpio_get_level(GPIO_INPUT);
}

void gpio_init() {
	gpio_config_t io_conf;
	//disable interrupt
	io_conf.intr_type = GPIO_INTR_DISABLE;
	//set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	//bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	//disable pull-down mode
	io_conf.pull_down_en = 0;
	//disable pull-up mode
	io_conf.pull_up_en = 0;
	//configure GPIO with the given settings
	gpio_config(&io_conf);

	gpio_set_direction(GPIO_INPUT, GPIO_MODE_INPUT);
	gpio_set_level(GPIO_OUTPUT_IO_1, 1);
}


