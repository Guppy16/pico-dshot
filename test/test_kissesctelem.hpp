#include "kissesctelem.h"
#include "unity.h"
#include <stdio.h>

const uint8_t kissesc_buffer[KISS_ESC_TELEM_BUFFER_SIZE] = {
    0x1c, 0x04, 0xd6, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x8f};

const uint8_t kissesc_buffer2[KISS_ESC_TELEM_BUFFER_SIZE] = {
    0x28, 0x05, 0x00, 0x00, 0x13, 0x00, 0x04, 0x03, 0x83, 0x13};

static void test_kissesc_get_crc8(void) {

  uint8_t crc8 = kissesc_get_crc8(kissesc_buffer, KISS_ESC_TELEM_BUFFER_SIZE);
  TEST_ASSERT_EQUAL(0x00, crc8);
  crc8 = kissesc_get_crc8(kissesc_buffer2, KISS_ESC_TELEM_BUFFER_SIZE);
  TEST_ASSERT_EQUAL(0x00, crc8);
}

static void test_kissesc_buffer_to_telem(void) {
  kissesc_telem_t telem_data;
  kissesc_buffer_to_telem(kissesc_buffer, &telem_data);
  kissesc_print_telem(&telem_data);

  kissesc_telem_t expected_telem{.temperature = 28,
                                 .centi_voltage = 1238,
                                 .centi_current = 0,
                                 .consumption = 23,
                                 .erpm = 0,
                                 .crc = 0};
  // kissesc_print_telem(&expected_telem);

  TEST_ASSERT_EQUAL(expected_telem.temperature, telem_data.temperature);
  TEST_ASSERT_EQUAL(expected_telem.centi_voltage, telem_data.centi_voltage);
  TEST_ASSERT_EQUAL(expected_telem.centi_current, telem_data.centi_current);
  TEST_ASSERT_EQUAL(expected_telem.consumption, telem_data.consumption);
  TEST_ASSERT_EQUAL(expected_telem.erpm, telem_data.erpm);
  TEST_ASSERT_EQUAL(expected_telem.crc, telem_data.crc);
}

static void test_kissesc_buffer2_to_telem(void) {
  kissesc_telem_t telem_data;
  kissesc_buffer_to_telem(kissesc_buffer2, &telem_data);
  kissesc_print_telem(&telem_data);

  kissesc_telem_t expected_telem{.temperature = 40,
                                 .centi_voltage = 1280,
                                 .centi_current = 19,
                                 .consumption = 4,
                                 .erpm = 89900,
                                 .crc = 0};
  // kissesc_print_telem(&expected_telem);

  TEST_ASSERT_EQUAL(expected_telem.temperature, telem_data.temperature);
  TEST_ASSERT_EQUAL(expected_telem.centi_voltage, telem_data.centi_voltage);
  TEST_ASSERT_EQUAL(expected_telem.centi_current, telem_data.centi_current);
  TEST_ASSERT_EQUAL(expected_telem.consumption, telem_data.consumption);
  TEST_ASSERT_EQUAL(expected_telem.erpm, telem_data.erpm);
  TEST_ASSERT_EQUAL(expected_telem.crc, telem_data.crc);
}

static void test_uint32_erpm(void) {
  // Check if uint32 < erpm < uint16 is supported
  uint8_t kissesc_buffer[KISS_ESC_TELEM_BUFFER_SIZE] = {0};
  kissesc_buffer[7] = 0xf0;
  kissesc_telem_t telem_data;
  kissesc_buffer_to_telem(kissesc_buffer, &telem_data);
  kissesc_print_telem(&telem_data);

  TEST_ASSERT_EQUAL(15 * (2 << 11) * 100, telem_data.erpm);
}

static int runUnityTests_kissesctelem(void) {
  UnityBegin("KISSESC_TELEM");
  RUN_TEST(test_kissesc_get_crc8);
  RUN_TEST(test_kissesc_buffer_to_telem);
  RUN_TEST(test_kissesc_buffer2_to_telem);
  RUN_TEST(test_uint32_erpm);
  return UNITY_END();
}