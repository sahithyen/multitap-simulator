#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "sio.pio.h"
#include "dualshock.h"

void pio0_irq0_isr(void);

#define PIO pio0
#define SM 0
#define BASE_PIN 2

uint8_t counter = 0;
uint reset_instruction = 0;

static inline void sio_program_init(uint offset);

void sio_init(void)
{
    // Setup interrupt
    irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq0_isr);
    irq_set_enabled(PIO0_IRQ_0, true);
    pio_set_irq0_source_enabled(PIO, pis_interrupt0, true);

    // Load program into instruction memory
    uint offset = pio_add_program(PIO, &sio_program);

    // Construct reset instruction
    reset_instruction = offset;

    // Initalize state machine
    sio_program_init(offset);

    // Start state machine
    pio_sm_set_enabled(PIO, SM, true);
}

static inline void sio_program_init(uint offset)
{
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
    pio_sm_set_consecutive_pindirs(PIO, SM, in_base, 3, false);
    pio_sm_set_consecutive_pindirs(PIO, SM, out_base, 2, true);

    // State machine config
    pio_sm_config c = sio_program_get_default_config(offset);

    // Set bases
    sm_config_set_in_pins(&c, in_base);
    sm_config_set_out_pins(&c, out_base, 1);
    sm_config_set_set_pins(&c, set_base, 1);

    // Configure OSR and ISR
    sm_config_set_in_shift(&c, true, true, 8);
    sm_config_set_out_shift(&c, true, true, 8);

    // Initialize state machine
    pio_sm_init(PIO, SM, offset, &c);
}

static inline void next_attention(void)
{
    counter = 0;
    pio_sm_exec(PIO, SM, reset_instruction);
    pio_sm_restart(PIO, SM);
}

void pio0_irq0_isr(void)
{
    uint8_t command = pio_sm_get(PIO, SM) >> 24;

    struct sio_descision *descision = dualshock_handle_sio(command);

    if (descision->next_packet)
    {
        next_attention();
    }
    else
    {
        pio_sm_put(PIO, SM, descision->data);
    }

    pio_interrupt_clear(PIO, 0);
}
