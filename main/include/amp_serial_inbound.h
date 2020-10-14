#ifndef INTERNAL_COMMUNICATION_AMP_SERIAL_H
#define INTERNAL_COMMUNICATION_AMP_SERIAL_H

#include <stdbool.h>
#include "amp_messages.h"

// UART1
#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)


void serial_init(void);

/**
 * Once the node has an assigned address, send a hello message to
 */
void amp_serial_hello(void *arg);

void amp_serial_inbound(void *arg);

int amp_serial_send(union amp_msg *msg, int byteCount);

#endif //INTERNAL_COMMUNICATION_AMP_SERIAL_H
