#pragma once
#include "stdint.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /** @file packet.h
   *  @defgroup dshot_packet dshot_packet
   *
   * Convert a Dshot command to a frame and packet,
   * with no hw includes, but assumes the packet is
   * transmit using a rpi pico PWM hw downstream.
   *
   * The DShot protocol requires a frame of 16 bits.
   * This is converted to a packet of length 20,
   * where the last 4 elements are an end of frame signal.
   * More info can be found in our readme:
   * https://github.com/Guppy16/pico-dshot
   *
   * Aside: This module can technically be reduced
   * to one function, because the user only uses
   * @ref dshot_cmd_to_packet()
   * Nevertheless, the functions have been segmented for ctest
   */

  const uint dshot_frame_size = 16;

  /**
   * @brief config for dshot packet, specific to pico pwm hw
   * @ingroup dshot_packet
   *
   * @param dshot_low duty cycle for a dshot low bit
   * @param dshot_high duty cycle for a dshot high bit
   * @param pwm_channel pico pwm channel.
   * See `hardware/pwm.h::pwm_gpio_to_channel`
   */
  typedef struct dshot_packet_config
  {
    const uint16_t dshot_high;
    const uint16_t dshot_low;
    const uint pwm_channel;
  } dshot_packet_config;

  /**
   * @brief Convert a DShot code and telemetry flag to a command
   *
   * @param code Dshot code 11 bits: 0 - 2047
   * @param t telemetry flag
   * @return uint16_t DShot command
   */
  static inline uint16_t dshot_code_telemtry_to_cmd(const uint16_t code, const uint16_t t)
  {
    return code << 1 | (t & 0x1);
  }

  /**
   * @brief Get the checksum of a Dshot command
   *
   * @param cmd Dshot command
   * @return uint16_t checksum
   *
   * @attention
   * A Dshot command is 12 bits: 11 bit code + 1 bit telemetry.
   * See @ref dshot_code_telemetry_to_cmd()
   */
  static inline uint16_t dshot_cmd_crc(const uint16_t cmd)
  {
    uint16_t crc = 0xF & (cmd ^ (cmd >> 4) ^ (cmd >> 8));
    return crc;
  }

  /**
   * @brief Convert a Dshot command to a frame
   *
   * @param cmd Dshot command
   * @return uint16_t Dshot frame
   *
   * @attention A Dshot frame is 16 bits:
   * 11 bit code + 1 bit telemetry + 4 bit checksum
   */
  static inline uint16_t dshot_cmd_to_frame(const uint16_t cmd)
  {
    uint16_t crc = dshot_cmd_crc(cmd);
    uint16_t frame = cmd << 4 | crc;
    return frame;
  }

  /**
   * @brief Convert Dshot frame to a packet and store in buffer
   *
   * @param frame dshot frame
   * @param packet_buffer array buffer to store packet.
   * This must be at least @ref `dshot_frame_size` long
   * @param conf dshot pwm buffer config
   *
   * @paragraph
   * Potentially, further optimisations can be done to this:
   * https://stackoverflow.com/questions/2249731/how-do-i-get-bit-by-bit-data-from-an-integer-value-in-c
   */
  static inline void dshot_frame_to_packet(uint16_t frame, uint32_t packet_buffer[], const dshot_packet_config &conf)
  {
    // See pico hardware/pwm.h` for why this shift is needed
    uint channel_shift = 16 * conf.pwm_channel;
    for (uint32_t b = 0; b < dshot_frame_size; ++b, frame <<= 1)
    {
      uint16_t duty_cycle = frame & 0x8000 ? conf.dshot_high : conf.dshot_low;
      packet_buffer[b] = (uint32_t)duty_cycle << channel_shift;
    }
  }

  /**
   * @brief Convert dshot code and telemetry to packet
   *
   * @param code dshot code
   * @param telemetry
   * @param packet_buffer array buffer to store packet.
   * This must be at least @ref `dshot_frame_size` long
   * @param conf dshot pwm buffer config
   */
  static inline void dshot_cmd_to_packet(const uint16_t code, const uint16_t telemetry, uint32_t packet_buffer[], const dshot_packet_config &conf)
  {
    uint16_t cmd = dshot_code_telemtry_to_cmd(code, telemetry);
    uint16_t frame = dshot_cmd_to_frame(cmd);
    dshot_frame_to_packet(frame, packet_buffer, conf);
  }

#ifdef __cplusplus
}
#endif