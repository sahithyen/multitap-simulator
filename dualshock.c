#include "dualshock.h"

#define DIGITAL 0x40
#define ANALOG 0x70
#define CONFIG 0xF0

struct sio_descision descision = {.next_packet = false, .data = 0};

// Controller state
bool escape = false;
bool is_digital = true;
bool led = true;
bool lock_analog = false;
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

uint8_t actuators_mapping[] = {
    0x00,
    0x01,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
};

uint8_t buttons_mapping[] = {
    0x3F,
    0x00,
    0x00};

uint8_t *buttons_mapped[18];

// Packet state
uint8_t packet_type = 0;
uint8_t packet_length = 0;
uint8_t byte_counter = 0;
uint8_t offset = 0;
uint8_t count = 0;

void dualshock_init(void)
{
    byte_counter = 0;

    escape = false;
    is_digital = true;
}

void handle_40h(uint8_t command)
{
    descision.data = 0x00;
    if (byte_counter == 4)
    {
        descision.data = 0x02;
    }
    if (byte_counter == 7)
    {
        descision.data = 0x5a;
    }
    if (byte_counter >= 8)
    {
        descision.next_packet = true;
    }
}

void handle_41h(uint8_t command)
{
    descision.data = 0x00;
    if (byte_counter >= 2 && byte_counter < 5)
    {
        descision.data = buttons_mapping[byte_counter - 2];
    }
    if (byte_counter == 7)
    {
        descision.data = 0x5a;
    }
    if (byte_counter >= 8)
    {
        descision.next_packet = true;
    }
}

void handle_42h_43h(uint8_t command)
{
    if (byte_counter == 3)
    {
        escape = packet_type == 0x43 && command == 0x01;
    }

    if (escape && byte_counter >= 8)
    {
        descision.next_packet = true;
    }
    else if (is_digital)
    {
        if (byte_counter < 4)
        {
            descision.data = input_state[byte_counter - 2];
        }
        else
        {
            descision.next_packet = true;
        }
    }
    else
    {
        descision.data = *buttons_mapped[byte_counter - 2];
    }
}

void handle_44h(uint8_t command)
{
    descision.data = 0;

    if (byte_counter == 3)
    {
        is_digital = command == 0x00;
    }
    else if (byte_counter == 4)
    {
        lock_analog = command == 0x03;
    }
    else if (byte_counter >= 8)
    {
        descision.next_packet = true;
    }
}

void handle_45h(uint8_t command)
{
    switch (byte_counter)
    {
    case 2:
        descision.data = 0x03;
        break;
    case 3:
        descision.data = 0x02;
        break;
    case 4:
        if (led)
        {
            descision.data = 0x01;
        }
        else
        {
            descision.data = 0x00;
        }
        break;
    case 5:
        descision.data = 0x02;
        break;
    case 6:
        descision.data = 0x01;
        break;
    case 7:
        descision.data = 0x00;
        break;
    default:
        descision.next_packet = true;
        break;
    }
}

void handle_46h(uint8_t command)
{
    descision.data = 0;
    if (byte_counter == 3)
    {
        offset = command;
    }
    if (byte_counter == 5 && offset == 0)
    {
        descision.data = 0x02;
    }
    if (byte_counter == 7)
    {
        if (offset == 1)
        {
            descision.data = 0x14;
        }
        else
        {
            descision.data = 0x0A;
        }
    }
    if (byte_counter >= 8)
    {
        descision.next_packet = true;
    }
}

void handle_47h(uint8_t command)
{
    descision.data = 0x00;
    if (byte_counter == 4)
    {
        descision.data = 0x02;
    }
    if (byte_counter >= 8)
    {
        descision.next_packet = true;
    }
}

void handle_4Ch(uint8_t command)
{
    descision.data = 0;
    if (byte_counter == 3)
    {
        offset = command;
    }
    if (byte_counter == 5)
    {
        descision.data = offset == 0 ? 0x04 : 0x06;
    }

    if (byte_counter >= 8)
    {
        descision.next_packet = true;
    }
}

void handle_4Dh(uint8_t command)
{

    if (byte_counter >= 2 && byte_counter <= 7)
    {
        descision.data = actuators_mapping[byte_counter - 2];
    }
    if (byte_counter >= 3)
    {
        actuators_mapping[byte_counter - 3] = command;
    }
    if (byte_counter >= 8)
    {
        descision.next_packet = true;
    }
}

void handle_4Fh(uint8_t command)
{
    if (byte_counter >= 3 && byte_counter < 6)
    {
        buttons_mapping[byte_counter - 3] = command;
    }
    descision.data = 0x00;
    if (byte_counter == 7)
    {
        descision.data = 0x5a;
    }

    if (byte_counter >= 8)
    {
        descision.next_packet = true;
    }
}

void dualshock_sio_packet_started(void)
{
    byte_counter = 0;
}

void dualshock_sio_packet_ended(void)
{
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
                uint32_t bi = buttons_mapping[0] | (buttons_mapping[1] << 8) | (buttons_mapping[2] << 16);

                packet_length = 0;

                for (size_t i = 0; i < 18; i++)
                {
                    if (bi & 1 == 1)
                    {
                        buttons_mapped[packet_length] = &input_state[i];

                        packet_length += 1;
                    }

                    bi = bi >> 1;
                }

                descision.data = ANALOG | (packet_length / 2);
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
        case 0x40:
            handle_40h(command);
            break;
        case 0x41:
            handle_41h(command);
            break;
        case 0x42:
        case 0x43:
            handle_42h_43h(command);
            break;
        case 0x44:
            handle_44h(command);
            break;
        case 0x45:
            handle_45h(command);
            break;
        case 0x46:
            handle_46h(command);
            break;
        case 0x47:
            handle_47h(command);
            break;
        case 0x4c:
            handle_4Ch(command);
            break;
        case 0x4d:
            handle_4Dh(command);
            break;
        case 0x4f:
            handle_4Fh(command);
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