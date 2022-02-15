#include <stdio.h>

#include "pico/stdlib.h"
#include "sio.h"

int main() {
    stdio_init_all();

    sio_init();

    while (true) ;
}
