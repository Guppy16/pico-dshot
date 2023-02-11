#include "dshot.h"

uint16_t DShot::dshot_code_telemtry_to_command(const uint16_t code, const uint16_t t)
{
  return code << 1 | t;
}

/*
Assumes tha cmd is a 12 bit number
(i.e. 11 bit DShot code + telemtry: CCCC CCCC CCCT)
*/
uint16_t DShot::command_to_crc(const uint16_t &cmd)
{
  uint16_t crc = 0xF & (cmd ^ (cmd >> 4) ^ (cmd >> 8));
  return crc;
}

/*
Assumes tha cmd is a 12 bit number
(i.e. 11 bit DShot code + telemtry: CCCC CCCC CCCT)
*/
uint16_t DShot::command_to_packet(const uint16_t &cmd)
{
  uint16_t crc = command_to_crc(cmd);
  uint16_t packet = cmd << 4 | crc;
  return packet;
}

void DShot::packet_to_pwm(const uint16_t &packet, uint32_t pwm_buffer[], const uint16_t &DShot_low, const uint16_t &DShot_high, const uint32_t &channel)
{
  uint32_t pwm_val = 0;
  unsigned int packet_size = 16;
  for (uint32_t b = 0; b < packet_size; ++b)
  {
    pwm_val = (uint32_t)(((packet << b) & 0x8000) ? DShot_high : DShot_low);
    pwm_buffer[b] = pwm_val << (16 * channel);
  }
}
