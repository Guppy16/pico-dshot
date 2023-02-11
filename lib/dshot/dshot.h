#pragma once
#include "inttypes.h"

/*
* Source file for common utility functions used with DShot
*/
namespace DShot
{
    uint16_t dshot_code_telemtry_to_command(const uint16_t code, const uint16_t t);

    uint16_t command_to_crc(const uint16_t &cmd);

    uint16_t command_to_packet(const uint16_t &cmd);

    void packet_to_pwm(const uint16_t &packet, uint32_t pwm_buffer[], const uint16_t &DShot_low, const uint16_t &DShot_high, const uint32_t &channel);
}