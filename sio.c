#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "sio.pio.h"
#include "dualshock.h"

void pio0_irq0_isr(void);

#define PIO pio0
#define PACKETS_SM 0
#define BYTES_SM 1
#define BASE_PIN 2

uint reset_bytes_instruction = 0;

static inline void sio_bytes_init();
static inline void sio_packets_init();

// Debug variables
bool dbg_new_packet = false;
uint8_t dbg_length = 0;
uint8_t dbg_rx[32];
uint8_t dbg_tx[32];

void sio_init(void)
{
    // Setup interrupt
    irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq0_isr);
    irq_set_enabled(PIO0_IRQ_0, true);
    pio_set_irq0_source_mask_enabled(
        PIO,
        (1 << pis_interrupt0) | (1 << pis_interrupt1) | (1 << pis_interrupt2),
        true);

    // Initialize sio_packets state machine
    sio_packets_init();

    // Initialize sio_bytes state machine
    sio_bytes_init();

    // Start packets state machine
    pio_sm_set_enabled(PIO, PACKETS_SM, true);
}

void sio_loop(void)
{
        if (dbg_new_packet)
        {
            printf("RX:");
            for (size_t i = 0; i < dbg_length; i++)
            {
                printf(" %02x", dbg_rx[i]);
            }
            printf("\n");

            printf("TX:");
            for (size_t i = 0; i < dbg_length; i++)
            {
                printf(" %02x", dbg_tx[i]);
            }
            printf("\n");

            dbg_new_packet = false;
        }
}

static inline void sio_packets_init()
{
    // Add program into instruction memory
    uint offset = pio_add_program(PIO, &sio_packets_program);

    // Calculate in base
    uint in_base = BASE_PIN + 1;

    // Calculate pins
    uint attention_pin = in_base;

    // Connect pin to PIO
    pio_gpio_init(PIO, attention_pin);

    // Set pin directions
    pio_sm_set_consecutive_pindirs(PIO, PACKETS_SM, in_base, 1, false);

    // State machine config
    pio_sm_config c = sio_packets_program_get_default_config(offset);

    // Set bases
    sm_config_set_in_pins(&c, in_base);

    // Initialize state machine
    pio_sm_init(PIO, PACKETS_SM, offset, &c);
}

static inline void sio_bytes_init()
{
    // Add program into instruction memory
    uint offset = pio_add_program(PIO, &sio_bytes_program);

    // Construct reset instruction
    reset_bytes_instruction = offset;

    // Calculate bases
    uint in_base = BASE_PIN;
    uint out_base = BASE_PIN + 3;
    uint set_base = BASE_PIN + 4;

    // Calculate pins
    uint command_pin = in_base;
    uint attention_pin = in_base + 1;
    uint clock_pin = in_base + 2;
    uint data_pin = out_base;
    uint acknowledge_pin = set_base;

    // Connect pins to PIO
    pio_gpio_init(PIO, command_pin);
    pio_gpio_init(PIO, attention_pin);
    pio_gpio_init(PIO, clock_pin);
    pio_gpio_init(PIO, data_pin);
    pio_gpio_init(PIO, acknowledge_pin);

    // Set pin directions
    pio_sm_set_consecutive_pindirs(PIO, BYTES_SM, in_base, 3, false);
    pio_sm_set_consecutive_pindirs(PIO, BYTES_SM, out_base, 2, true);

    // State machine config
    pio_sm_config c = sio_bytes_program_get_default_config(offset);

    // Set bases
    sm_config_set_in_pins(&c, in_base);
    sm_config_set_out_pins(&c, out_base, 1);
    sm_config_set_set_pins(&c, set_base, 1);

    // Configure OSR and ISR
    sm_config_set_in_shift(&c, true, true, 8);
    sm_config_set_out_shift(&c, true, true, 8);

    // Initialize state machine
    pio_sm_init(PIO, BYTES_SM, offset, &c);
}

static inline void next_attention(void)
{
    pio_sm_exec(PIO, BYTES_SM, reset_bytes_instruction);
    pio_sm_restart(PIO, BYTES_SM);
}

void pio0_irq0_isr(void)
{
    if (pio_interrupt_get(PIO, 0))
    {
        // PS2 will end packets prematurely
        pio_sm_clear_fifos(PIO, BYTES_SM);

        // Cable plugged off in midst of an byte
        pio_sm_restart(PIO, BYTES_SM);

        // Go to beginning of the program
        pio_sm_exec_wait_blocking(PIO, BYTES_SM, reset_bytes_instruction);

        pio_sm_set_enabled(PIO, BYTES_SM, true);

        dualshock_sio_packet_started();

        dbg_length = 0;
        dbg_tx[0] = 0xFF;

        pio_interrupt_clear(PIO, 0);
    }
    else if (pio_interrupt_get(PIO, 1))
    {
        uint8_t command = pio_sm_get(PIO, BYTES_SM) >> 24;

        dbg_rx[dbg_length] = command;
        dbg_length++;

        struct sio_descision *descision = dualshock_sio_received_byte(command);

        if (descision->next_packet)
        {
            pio_sm_set_enabled(PIO, BYTES_SM, false);
        }
        else
        {
            pio_sm_put(PIO, BYTES_SM, descision->data);
            dbg_tx[dbg_length] = descision->data;
        }

        pio_interrupt_clear(PIO, 1);
    }
    else if (pio_interrupt_get(PIO, 2))
    {
        dbg_new_packet = true;

        dualshock_sio_packet_ended();

        pio_sm_set_enabled(PIO, BYTES_SM, false);

        pio_interrupt_clear(PIO, 2);
    }
}
