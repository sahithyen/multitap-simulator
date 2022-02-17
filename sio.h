#include "pico/stdlib.h"

struct sio_descision
{
    uint8_t data;
    bool next_packet;
};

void sio_init(void);
void sio_loop(void);
