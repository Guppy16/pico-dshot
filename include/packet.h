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

  #define dshot_packet_length 20
  const uint dshot_frame_size = 16;

  /**
   * @brief config used for composing dshot packet
   * @ingroup dshot_packet
   *
   * @param primary_packet_buffer
   * ~~@param secondary_packet_buffer This is declared here, but is used in another file~~
   * @param throttle_code dshot throttle code (11 bit)
   * @param telemetry dshot telemetry flag (1 bit)
   * @param packet_high duty cycle for a dshot high bit
   * @param packet_low duty cycle for a dshot low bit
   * ~~@param pwm_channel pico pwm channel for dshot packet transmission~~
   *
   * @attention
   * Normally, the packet_high or packet_low would be 16 bits,
   * because the pico timers are 16 bit.
   * However, the pico uses 32 bits to store the value of two timers,
   * each half stores the timer for a different pwm "channel".
   * Hene the value of packet_high and packet_low should be initialised as follows:
   * ```
   *  packet_high = (uint32_t)(0.75 * pwm_wrap) << (16 * pwm_channel)
   *  packet_low = (uint32_t)(0.37 * pwm_wrap) << (16 * pwm_channel)
   * ```
   * More details about this shift for pwm_channel can be found in pico hardware/pwm
   */
  typedef struct dshot_packet
  {
    uint32_t volatile packet_buffer[dshot_packet_length];
    // uint32_t volatile packet_buffer[];
    // uint32_t volatile secondary_packet_buffer[dshot_packet_length] = {0};
    uint16_t throttle_code;
    uint16_t telemetry;
    uint32_t packet_high;
    uint32_t packet_low;
    // const uint pwm_channel;
  } dshot_packet_t;

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
   * @param packet_buffer array buffer to store \a packet.
   * This must be at least @ref `dshot_frame_length` long
   * NOTE: no checks are done for this!
   * @param packet_high duty cycle for a high bit
   * @param packet_low duty cycle for a low bit
   * @param dshot_frame_length length of \a frame to convert. Default is 16
   *
   * @paragraph
   * Potentially, further optimisations can be done to this:
   * https://stackoverflow.com/questions/2249731/how-do-i-get-bit-by-bit-data-from-an-integer-value-in-c
   */
  static inline void dshot_frame_to_packet(uint16_t frame, uint32_t volatile packet_buffer[], const uint32_t packet_high, const uint32_t packet_low)
  {
    // Convert each bit in the frame to a high / low duty cycles in the packet
    for (uint32_t b = 0; b < dshot_frame_size; ++b, frame <<= 1)
    {
      packet_buffer[b] = frame & 0x8000 ? packet_high : packet_low;
    }
  }

  /**
   * @brief Compose packet from dshot code and telemetry
   *
   * @param dshot_pckt dshot_packet_t config.
   * The calculated packet is stroed in dshot_pckt.packet_buffer
   */
  static inline void dshot_packet_compose(dshot_packet_t *dshot_pckt)
  {
    uint16_t cmd = dshot_code_telemtry_to_cmd(dshot_pckt->throttle_code, dshot_pckt->telemetry);
    uint16_t frame = dshot_cmd_to_frame(cmd);
    dshot_frame_to_packet(frame, dshot_pckt->packet_buffer, dshot_pckt->packet_high, dshot_pckt->packet_low);
  }

#ifdef __cplusplus
}
#endif