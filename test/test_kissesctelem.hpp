#include "kissesctelem.h"
#include "unity.h"
#include <stdio.h>

static void test_kissesc_get_crc8(void) {
  uint8_t buffer[KISS_ESC_TELEM_BUFFER_SIZE] = {0x1c, 0x04, 0xd6, 0x00, 0x00,
                                                0x00, 0x17, 0x00, 0x00, 0x8f};
  uint8_t crc = kissesc_get_crc8(buffer, KISS_ESC_TELEM_BUFFER_SIZE);
  printf("CRC: %.2x\n", crc);

  kissesc_telem telem;
  kissesc_buffer_to_telem(buffer, &telem);
  kissesc_print(&telem);
}

static void test_kissesc_buffer_to_telem(void) {
  uint8_t buffer[KISS_ESC_TELEM_BUFFER_SIZE] = {0};
  kissesc_telem telem;
  kissesc_buffer_to_telem(buffer, &telem);
  kissesc_print(&telem);
}

static int runUnityTests_kissesctelem(void) {
  UnityBegin("KISSESCTELEM");
  RUN_TEST(test_kissesc_get_crc8);
  RUN_TEST(test_kissesc_buffer_to_telem);
  return UNITY_END();
}