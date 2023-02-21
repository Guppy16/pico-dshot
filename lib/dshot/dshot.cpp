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

  // This is another interesting way to do it (assuming packet is passed as a rvalue)
  // Source: https://stackoverflow.com/questions/2249731/how-do-i-get-bit-by-bit-data-from-an-integer-value-in-c#:~:text=can%20be%20further-,changed,-to
  // for (uint32_t b = 0; b < packet_size; ++b, packet <<= 1)
  // {
  //   pwm_val = (uint32_t)(((packet << b) & 0x8000) ? DShot_high : DShot_low);
  //   pwm_buffer[b] = pwm_val << (16 * channel);
  // }
}

/*
Helper function to convert a DShot code and telemetry
to PWM values in a buffer
*/
void DShot::command_to_pwm_buffer(const uint16_t &code, const uint16_t &telemetry, uint32_t pwm_buffer[], const uint16_t &DShot_low, const uint16_t &DShot_high, const uint32_t &channel){
  // Get command from DShot code and telemetry
  uint16_t cmd = DShot::dshot_code_telemtry_to_command(code, telemetry);
  // Convert command to packet
  uint16_t packet = DShot::command_to_packet(cmd);
  // Convert packet to PWM values in buffer
  DShot::packet_to_pwm(packet, pwm_buffer, DShot_low, DShot_high, channel);
}
