#ifndef INTERNAL_COMMUNICATION_AMP_MESSAGES_H
#define INTERNAL_COMMUNICATION_AMP_MESSAGES_H

#include <stdint.h>

#define AMP_MESSAGE_SIZE    (1024)

/* -------- Addressing Messages -------- */
#define AMP_MSGT_A_POOL_ADVERTISEMENT       (0xA1)
#define AMP_MSGT_A_POOL_ACCEPTED            (0xA2)
#define AMP_MSGT_A_POOL_ASSIGNED            (0xA3)
#define AMP_MSGT_A_POOL_REVOKED             (0xA4)
#define AMP_MSGT_A_BIN_CAPACITY_REQ         (0xA5)
#define AMP_MSGT_A_BIN_CAPACITY_REPL        (0xA6)

/* -------- Control Messages -------- */
#define AMP_MSGT_C_HELLO                    (0xC1)
#define AMP_MSGT_C_GOODBYE                  (0xC2)
#define AMP_MSGT_C_GOODBYE_ACK              (0xC3)

/* -------- Data Messages -------- */
#define AMP_MSGT_D_DATAGRAM                 (0xD1)
#define AMP_MSGT_D_ACKNOWLEDGED_DATAGRAM    (0xD2)
#define AMP_MSGT_D_DATAGRAM_ACK             (0xD3)

/* -------- Routing Messages -------- */
#define AMP_MSGT_R_ROUTE_DISCOVERY          (0xF1)
#define AMP_MSGT_R_ROUTE_REPLY              (0xF2)

/* -------- utility typedefs -------- */
// defines length of address
typedef uint64_t address;

// header field
typedef uint8_t msg_type;

// Used in addressing messages
typedef struct {
	address start_address;
	uint64_t pool_size;
} address_pool;

/* -------- Common Header -------- */
typedef struct {
	msg_type msg_type;
	address source;
	address destination;
} amp_msg_header;

typedef struct {
	msg_type msg_type;
	address source;
	address destination;
	uint8_t hop_count;
	uint8_t hop_limit;
} amp_msg_forwardable_header;

/* -------- Prototype messages -------- */
typedef struct {
	amp_msg_header header;
} amp_msg_t;

typedef struct {
	amp_msg_forwardable_header header;
} amp_msg_forwardable_t;

/* -------- Addressing Messages -------- */
typedef struct {
	amp_msg_header header;
	uint8_t address_pool_count;
	address_pool pools[62];
} amp_msg_a_pool_advertisement, amp_msg_a_pool_assigned, amp_msg_a_pool_revoked;

typedef struct {
	amp_msg_header header;
} amp_msg_a_pool_accepted, amp_msg_a_bin_cap_request;

typedef struct {
	amp_msg_header header;
	uint64_t bin_capacity;
} amp_msg_a_bin_cap_reply;

/* -------- Control Messages -------- */
typedef struct {
	amp_msg_header header;
} amp_msg_c_hello, amp_msg_c_bye, amp_msg_c_bye_ack;


/* -------- Data Messages -------- */
typedef struct {
	amp_msg_forwardable_header header;
	uint16_t payload_length;
	uint8_t payload[1003];
} amp_msg_d_datagram;

typedef struct {
	amp_msg_forwardable_header header;
	uint16_t identification_code;
	uint16_t payload_length;
	uint8_t payload[1001];
} amp_msg_d_acknowledged_datagram;

typedef struct {
	amp_msg_forwardable_header header;
	uint16_t identification_code;
} amp_msg_d_datagram_ack;


/* -------- Routing Messages -------- */
typedef struct {
	amp_msg_forwardable_header header;
} amp_msg_r_route_discovery, amp_msg_r_route_reply;


/* -------- Union for unified message handling -------- */
union amp_msg {
	// generic message to access header
	amp_msg_t generic_msg;

	// addressing messages
	amp_msg_a_pool_advertisement pool_advertisement;
	amp_msg_a_pool_accepted pool_accepted;
	amp_msg_a_pool_assigned pool_assigned;

	amp_msg_a_bin_cap_request bin_cap_request;
	amp_msg_a_bin_cap_reply bin_cap_reply;

	//control messages
	amp_msg_c_hello hello;
	amp_msg_c_bye bye;
	amp_msg_c_bye_ack bye_ack;

	//data messages
	amp_msg_d_datagram datagram;
	amp_msg_d_acknowledged_datagram acknowledged_datagram;
	amp_msg_d_datagram_ack datagram_ack;

	// routing messages
	amp_msg_r_route_discovery route_discovery;
	amp_msg_r_route_reply route_reply;

	// generic byte buffer to access raw content
	uint8_t buffer[AMP_MESSAGE_SIZE];
};

union amp_msg_forwardable {

	// generic message to access header
	amp_msg_forwardable_t generic_forwardable_msg;

	//data messages
	amp_msg_d_datagram datagram;
	amp_msg_d_acknowledged_datagram acknowledged_datagram;
	amp_msg_d_datagram_ack datagram_ack;

	// routing messages
	amp_msg_r_route_discovery route_discovery;
	amp_msg_r_route_reply route_reply;

	// generic byte buffer to access raw content
	uint8_t buffer[AMP_MESSAGE_SIZE];
};

#endif //INTERNAL_COMMUNICATION_AMP_MESSAGES_H
