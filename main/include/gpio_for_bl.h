#ifndef ESP_AMP_GPIO_FOR_BL_H
#define ESP_AMP_GPIO_FOR_BL_H

#define GPIO_OUTPUT_IO_0    2
#define GPIO_OUTPUT_IO_1    12
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0)|(1ULL<<GPIO_OUTPUT_IO_1))

#define GPIO_INPUT          13

void gpio_init();

bool isNodeGateway();

void toggleOutput();

#endif //ESP_AMP_GPIO_FOR_BL_H
