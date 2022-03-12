#pragma once

#include "pico/stdlib.h"
#include "sio.h"

void dualshock_init(void);

void dualshock_sio_packet_started(void);
struct sio_descision *dualshock_sio_received_byte(uint8_t command);
void dualshock_sio_packet_ended(void);

extern uint8_t input_state[18];
