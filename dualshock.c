#include "dualshock.h"

#define DIGITAL 0x40
#define ANALOG 0x70
#define CONFIG 0xF0

struct sio_descision descision = {.next_packet = false, .data = 0};

// Controller state
bool escape = false;
bool is_digital = true;
uint8_t input_state[18] = {
    0b11111111,
    0b11111111,
    127,
    127,
    127,
    127,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

// Packet state
uint8_t packet_type = 0;
uint8_t packet_length = 0;
uint8_t byte_counter = 0;

void dualshock_init(void)
{
    byte_counter = 0;

    escape = false;
    is_digital = true;
}

void handle_42h_43h(uint8_t command)
{
    if (byte_counter == 3) {
        escape = packet_type == 43 && command == 0x01;
    }

    if (escape && byte_counter >= 8) {
        descision.next_packet = true;
    } else if (is_digital) {
        if (byte_counter < 4) {
            descision.data = input_state[byte_counter - 2];
        } else {
            descision.next_packet = true;
        }
    } else {
        // TODO: Implement
        descision.next_packet = true;
    }
}

void dualshock_sio_packet_started(void) {
    byte_counter = 0;
}

void dualshock_sio_packet_ended(void) {
}

struct sio_descision *dualshock_sio_received_byte(uint8_t command)
{
    descision.next_packet = false;

    if (byte_counter == 0)
    {
        // First byte needs to be 1 otherwise something is wrong
        if (command == 0x01)
        {
            if (escape)
            {
                packet_length = 6;
                descision.data = CONFIG | (packet_length / 2);
                
            }
            else if (is_digital)
            {
                packet_length = 2;
                descision.data = DIGITAL | (packet_length / 2);
            }
            else
            {
                // TODO: Implement
                descision.next_packet = true;
            }
        }
        else
        {
            // TODO: Failure
            descision.next_packet = true;
        }
    }
    else if (byte_counter == 1)
    {
        packet_type = command;

        if (packet_type != 0x42 && packet_type != 0x43 && !escape)
        {
            // TODO: Failure
            descision.next_packet = true;
        }
        else
        {
            descision.data = 0x5a;
        }
    }
    else
    {
        switch (packet_type)
        {
        case 0x43:
        case 0x42:
            handle_42h_43h(command);
            break;
        
        default:
            // TODO: Implement
            descision.next_packet = true;
            break;
        }
    }

    byte_counter++;

    return &descision;
}