#include <stdio.h>

#include "pico/stdlib.h"
#include "dualshock.h"

int main()
{
    stdio_init_all();

    dualshock_init();

    sio_init();

    while (true)
        ;
}
