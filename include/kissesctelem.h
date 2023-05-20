/**
 * @file kissesctelem.h
 * @brief Functions used to process onewire KISS ESC telemetry data
 *
 * Sources: KISS esc telemetry datasheet:
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
  uint8_t crc;
} kissesc_telem_t;

/**
 * @brief Helper function to update running total for CRC8
 *
 * @param val current set of bytes
 * @param crc_seed previously calculated CRC8 (initial value should be 0)
 * @return uint8_t CRC update
 *
 * @note Implementation was taken from KISS ESC telemetry datasheet
 * @attention This implementation could be improved by pre-calculating and
 * storing in a LUT
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
 * @brief Calculate CRC8 from a buffer
 *
 * @param buffer
 * @param buffer_size KISS_ESC_TELEM_BUFFER_SIZE
 * @return uint8_t CRC checksum. 0 => checksum was good
 */
static inline uint8_t kissesc_get_crc8(const volatile uint8_t buffer[],
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
 * @param telem_data
 */
static inline void
kissesc_buffer_to_telem(const volatile uint8_t buffer[KISS_ESC_TELEM_BUFFER_SIZE],
                        volatile kissesc_telem_t *const telem_data) {
  telem_data->temperature = buffer[0];
  telem_data->centi_voltage = ((uint16_t)(buffer[1])) << 8 | (uint16_t)(buffer[2]);
  telem_data->centi_current = ((uint16_t)(buffer[3])) << 8 | (uint16_t)(buffer[4]);
  telem_data->consumption = ((uint16_t)(buffer[5])) << 8 | (uint16_t)(buffer[6]);
  telem_data->erpm = 100 * ((uint32_t)(buffer[7])) << 8 |
                (uint32_t)(buffer[8]); // 32 bit so no overflow after x100
  telem_data->crc = kissesc_get_crc8(buffer, KISS_ESC_TELEM_BUFFER_SIZE);
}

static void kissesc_print_buffer(const volatile uint8_t buffer[],
                                 const size_t buffer_size) {
  printf("Buffer:\t0x");
  for (size_t i = 0; i < buffer_size; ++i) {
    printf("%.2x", buffer[i]);
  }
  printf("\n");
}

static void kissesc_print_telem(const volatile kissesc_telem_t *telem_data) {
  printf("\n---KISS ESC TELEMETRY---\n");
  printf("Temperature:\t%i C\n", telem_data->temperature);
  printf("Voltage:\t%.2f V\n", telem_data->centi_voltage / 100.0f);
  printf("Current:\t%.2f A\n", telem_data->centi_current / 100.0f);
  printf("Consumption:\t%i mAh\n", telem_data->consumption);
  printf("Erpm:\t\t%i\n", telem_data->erpm);
  printf("CRC8:\t\t0x%.2x\n", telem_data->crc);
}

#ifdef __cplusplus
}
#endif