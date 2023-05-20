/**
 * @file kissesctelem.h
 * @brief
 *
 * Implementation of functionality used to process KISS ESC telemetry data
 * Sources:
 * KISS esc telemetry datasheet:
 * https://www.rcgroups.com/forums/showatt.php?attachmentid=8524039&d=1450424877
 *
 * A general explanation on how CRC8 works can be found here:
 * http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html#ch4
 * I think this algo is used; TODO: figure out the divisor (10?)
 */

#pragma once
#include "stdint.h"
#include "stdio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Buffer size of telemetry is 10 bytes for KISS ESC protocol
#define KISS_ESC_TELEM_BUFFER_SIZE 10

/**
 * @brief struct to define telemetry data from kiss esc onewire protocol
 *
 * NOTE: to convert from erpm -> rpm:
 * rpm = erpm / (magnetpole / 2)
 */
typedef struct kissesc_telem {
  int8_t temperature;     // 1 C
  uint16_t centi_voltage; // 1/100 th of 1 V
  uint16_t centi_current; // 1/100th of 1 A
  uint16_t consumption;   // 1 mAh
  uint32_t erpm;
} kissesc_telem;

/**
 * @brief Update running total for CRC
 *
 * @param val current set of bytes
 * @param crc_seed previously calculated CRC8 (initial value should be 0)
 * @return uint8_t CRC update
 *
 * @note Implementation was taken from KISS ESC telemetry datasheet
 * @attention This implementation could be improved by pre-calculating and
 * sotring in a LUT
 */
static inline uint8_t kissesc_update_crc(const uint8_t val,
                                         const uint8_t crc_seed) {
  uint8_t crc = val ^ crc_seed; // XOR val with crc_seed to determine crc
  // Process crc
  for (int i = 0; i < 8; ++i) {
    crc = (crc & 0x80) ? 0x7 ^ (crc << 1) : (crc << 1);
  }
  return crc;
}

/**
 * @brief Calculate CRC from a buffer
 * 
 * @param buffer 
 * @param buffer_size KISS_ESC_TELEM_BUFFER_SIZE
 * @return uint8_t CRC checksum. 0 => checksum was good
 */
static inline uint8_t kissesc_get_crc8(const uint8_t buffer[],
                                       uint8_t buffer_size) {
  uint8_t crc = 0;
  for (int i = 0; i < buffer_size; ++i) {
    crc = kissesc_update_crc(buffer[i], crc);
  }
  return crc;
}

/**
 * @brief Convert buffer to telemetry data
 * 
 * @param buffer 
 * @param telem 
 */
static inline void
kissesc_buffer_to_telem(const uint8_t buffer[KISS_ESC_TELEM_BUFFER_SIZE],
                        kissesc_telem *const telem) {
  telem->temperature = buffer[0];
  telem->centi_voltage = ((uint16_t)(buffer[1])) << 8 | (uint16_t)(buffer[2]);
  telem->centi_current = ((uint16_t)(buffer[3])) << 8 | (uint16_t)(buffer[4]);
  telem->consumption = ((uint16_t)(buffer[5])) << 8 | (uint16_t)(buffer[6]);
  telem->erpm =
      100 * ((uint32_t)(buffer[7])) << 8 |
      (uint32_t)(buffer[8]); // 32 bit to support multiplication by 100
}

static void kissesc_print(kissesc_telem *const telem) {
  printf("---KISS ESC TELEMTRY---\n");
  printf("Temperature:\t%i C\n", telem->temperature);
  printf("Voltage:\t%.2f V\n", telem->centi_voltage / 100.0f);
  printf("Current:\t%.2f A\n", telem->centi_current / 100.0f);
  printf("Consumption:\t%i mAh\n", telem->consumption);
  printf("erpm:\t%i\n", telem->erpm);
}

#ifdef __cplusplus
}
#endif