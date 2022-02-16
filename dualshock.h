#include "pico/stdlib.h"
#include "sio.h"

void dualshock_init(void);
struct sio_descision *dualshock_handle_sio(uint8_t command);
