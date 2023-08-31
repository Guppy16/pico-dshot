#include "kissesctelem.h"
#include "unity.h"
#include <stdio.h>

const size_t num_params = 2;

const uint8_t kissesc_buffers[num_params][KISS_ESC_TELEM_BUFFER_SIZE] = {
    {0x1c, 0x04, 0xd6, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x8f},
    {0x28, 0x05, 0x00, 0x00, 0x13, 0x00, 0x04, 0x03, 0x83, 0x13}};

const kissesc_telem_t expected_telems[num_params]{{.temperature = 28,
                                                   .centi_voltage = 1238,
                                                   .centi_current = 0,
                                                   .consumption = 23,
                                                   .erpm = 0,
                                                   .crc = 0},
                                                  {.temperature = 40,
                                                   .centi_voltage = 1280,
                                                   .centi_current = 19,
                                                   .consumption = 4,
                                                   .erpm = 89900,
                                                   .crc = 0}};

static void test_kissesc_get_crc8(void) {
  for (const auto &kb : kissesc_buffers) {
    uint8_t crc8 = kissesc_get_crc8(kb, KISS_ESC_TELEM_BUFFER_SIZE);
    TEST_ASSERT_EQUAL(0x00, crc8);
  }
}

static void test_kissesc_buffer_to_telem(void) {
  for (size_t i = 0; i < num_params; ++i) {
    kissesc_telem_t telem_data;
    kissesc_buffer_to_telem(kissesc_buffers[i], &telem_data);

    // Debugging outputs
    kissesc_print_telem(&telem_data);
    // kissesc_print_telem(&expected_telems[i]);

    TEST_ASSERT_EQUAL(expected_telems[i].temperature, telem_data.temperature);
    TEST_ASSERT_EQUAL(expected_telems[i].centi_voltage,
                      telem_data.centi_voltage);
    TEST_ASSERT_EQUAL(expected_telems[i].centi_current,
                      telem_data.centi_current);
    TEST_ASSERT_EQUAL(expected_telems[i].consumption, telem_data.consumption);
    TEST_ASSERT_EQUAL(expected_telems[i].erpm, telem_data.erpm);
    TEST_ASSERT_EQUAL(expected_telems[i].crc, telem_data.crc);
  }
}

static void test_uint32_erpm(void) {
  // Check if uint32 < erpm < uint16 is supported
  uint8_t kissesc_buffer[KISS_ESC_TELEM_BUFFER_SIZE] = {0};

  // Artificially set the erpm data to 0xf000 = 15 * 16^3
  // i.e. byte 7 = 0xf0, byte 8 = 0x00
  kissesc_buffer[7] = 0xf0;
  kissesc_telem_t telem_data;
  kissesc_buffer_to_telem(kissesc_buffer, &telem_data);
  kissesc_print_telem(&telem_data);

  TEST_ASSERT_EQUAL(15 * (1 << 12) * 100, telem_data.erpm);
}

static int runUnityTests_kissesctelem(void) {
  UnityBegin("KISSESC_TELEM");
  RUN_TEST(test_kissesc_get_crc8);
  RUN_TEST(test_kissesc_buffer_to_telem);
  RUN_TEST(test_uint32_erpm);
  return UNITY_END();
}