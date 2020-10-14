#ifndef INTERNAL_COMMUNICATION_MESH_INTERNALS_H
#define INTERNAL_COMMUNICATION_MESH_INTERNALS_H

#include "amp_messages.h"

/*******************************************************
 *                Function Declarations
 *******************************************************/

void init_mesh(void);

void mesh_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/**
 * send message via layer 2
 * @param msg message to send
 * @param destination AMP message or all null when to be send via serial
 */
void send_amp_message_via_layer_2(union amp_msg *msg, mesh_addr_t *destination);

void send_amp_message(union amp_msg *msg);
/*******************************************************
 *                Constants
 *******************************************************/
#define RX_SIZE          (1500)

/*******************************************************
 *                Constants
 *******************************************************/
static esp_netif_t *netif_sta = NULL;

extern mesh_addr_t mesh_parent_addr;
#endif //INTERNAL_COMMUNICATION_MESH_INTERNALS_H
