#ifndef INTERNAL_COMMUNICATION_AMP_MSG_RESOLVER_H
#define INTERNAL_COMMUNICATION_AMP_MSG_RESOLVER_H

#include "amp_messages.h"
#include "esp_mesh.h"
#include "amp_globals.h"

bool compareLayer2Addresses(uint8_t *a, uint8_t *b);

void amp_msg_handler(void *arg);

#endif //INTERNAL_COMMUNICATION_AMP_MSG_RESOLVER_H
