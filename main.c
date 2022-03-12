#include <stdio.h>

#include "pico/stdlib.h"
#include "sio.h"
#include "dualshock.h"

int main()
{
    stdio_uart_init_full(uart0, 921600, 1, 2);

    dualshock_init();

    sio_init();

    uint8_t packet_index = 0;
    bool skip = false;
    while (true)
    {
        // Master handling
        int16_t result = getchar_timeout_us(10);

        if (result != PICO_ERROR_TIMEOUT)
        {
            uint8_t byte = (uint8_t)result;
            if (skip)
            {
                if (byte == '\n')
                {
                    packet_index = 0;
                    skip = false;
                }
            }
            else
            {
                if (packet_index == 0)
                {
                    if (byte == 'b')
                    {
                        packet_index++;
                    }
                    else
                    {
                        skip = true;
                    }
                }
                else if (packet_index < 19)
                {
                    input_state[packet_index - 1] = byte;
                    packet_index++;
                }
                else
                {
                    packet_index = 0;
                }
            }
        }

        // Debug output
        sio_loop();
    }
}
