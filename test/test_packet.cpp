#include "unity.h"
#include "packet.h"
#include <stdio.h>

void setUp(void)
{
  // set stuff up here
}

void tearDown(void)
{
  // clean stuff up here
}

/**
 * @brief Test \a dshot_code_telemetry_to_cmd
 *
 * code = 1046 (0x416), telemetry = 0
 * command = 0x416 x 2 + 0 = 0x82C
 *
 * code = 1046 (0x416), telemetry = 1
 * command = 0x416 x 2 + 1 = 0x82D
 */
void test_dshot_code_telemtry_to_cmd(void)
{
  uint16_t code = 1046;
  uint16_t telemetry, expected_cmd, actual_cmd;

  telemetry = 0;
  expected_cmd = 0x82C;
  actual_cmd = dshot_code_telemetry_to_cmd(code, telemetry);
  TEST_ASSERT_EQUAL_MESSAGE(expected_cmd, actual_cmd, "Telemetry: 0");

  telemetry = 1;
  expected_cmd = 0x82D;
  actual_cmd = dshot_code_telemetry_to_cmd(code, telemetry);
  TEST_ASSERT_EQUAL_MESSAGE(expected_cmd, actual_cmd, "Telemetry: 1");
}

/**
 * @brief Test \a dshot_cmd_to_crc
 *
 * code = 1046 (0x416), telemetry = 0
 * --> command = 0x82C
 * --> crc = 0x6 = 0b0110
 */
void test_dshot_cmd_crc(void)
{
  uint16_t crc = dshot_cmd_crc(1046 << 1);
  TEST_ASSERT_EQUAL(0b0110, crc);
}

/**
 * @brief Test \a dshot_cmd_to_frame
 *
 * code = 1046 (0x416), telemetry = 0
 * --> command = 0x82C
 * --> crc = 6
 * frame = 0x82C6 = 0b1000001011000110
 */
void test_dshot_cmd_to_frame(void)
{
  uint16_t frame = dshot_cmd_to_frame(1046 << 1);
  TEST_ASSERT_EQUAL(0b1000001011000110, frame);
}

/**
 * @brief test @a dshot_frame_to_packet
 *
 * Test with these cases:
 *    frame = 0, high = 75, low = 33
 *    frame = 1, high = 75, low = 33
 *    frame = 1, high = 75 << 16, low = 33 << 16
 */
void test_dshot_frame_to_packet(void)
{
  // Frame is any 16 bit number
  uint16_t frame;
  uint32_t packet[16] = {0};
  uint32_t expected_packet[16];
  const uint32_t dshot_frame_length = 16;

  // pwm channel = 0 (i.e. there is no bit shift in packet values)
  uint32_t pulse_high = 75;
  uint32_t pulse_low = 33;

  // frame = 0
  frame = 0b0;
  for (int i = 0; i < 16; ++i)
  {
    expected_packet[i] = pulse_low;
  }
  dshot_frame_to_packet(frame, packet, pulse_high, pulse_low, dshot_frame_length);
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_packet, packet, 16, "frame = 0b0, Channel 0");

  // frame = 1
  frame = 0b1;
  for (int i = 0; i < 16; ++i)
  {
    expected_packet[i] = pulse_low;
  }
  expected_packet[16 - 1] = pulse_high;
  dshot_frame_to_packet(frame, packet, pulse_high, pulse_low, dshot_frame_length);
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_packet, packet, 16, "frame = 0b1, Channel 0");

  // frame = 1, with bit shifted values for pulse_high and pulse_low
  frame = 0b1;

  pulse_high = 75 << 16;
  pulse_low = 33 << 16;

  for (int i = 0; i < 16; ++i)
  {
    expected_packet[i] = pulse_low;
  }
  expected_packet[16 - 1] = pulse_high;
  dshot_frame_to_packet(frame, packet, pulse_high, pulse_low, dshot_frame_length);
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_packet, packet, 16, "frame = 0b1, channel 1");
}

/**
 * @brief test dshot_packet_compose
 *
 * Test parameters:
 *    code = 1, telemetry = 1
 *    --> expected frame = 0x0033
 *    expected packet = LLLL LLLL LLHH LLHH 0000
 */
void test_dshot_packet_compose(void)
{
  uint32_t pulse_high = 75, pulse_low = 33;

  dshot_packet_t dshot_pckt = {
      .throttle_code = 1,
      .telemetry = 1,
      .pulse_high = pulse_high,
      .pulse_low = pulse_low};

  // Construct expected packet:
  uint32_t expected_packet[] = {
      // LLLL
      pulse_low, pulse_low, pulse_low, pulse_low,
      // LLLL
      pulse_low, pulse_low, pulse_low, pulse_low,
      // LLHH
      pulse_low, pulse_low, pulse_high, pulse_high,
      // LLHH
      pulse_low, pulse_low, pulse_high, pulse_high,
      // 0000
      0, 0, 0, 0};

  dshot_packet_compose(dshot_pckt);

  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_packet, dshot_pckt.packet_buffer, dshot_packet_length, "Code = 1, Telemetry = 1, Channel 0, Array idx: 0");
}

int runUnityTests(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_dshot_code_telemtry_to_cmd);
  RUN_TEST(test_dshot_cmd_crc);
  RUN_TEST(test_dshot_cmd_to_frame);
  RUN_TEST(test_dshot_frame_to_packet);
  RUN_TEST(test_dshot_packet_compose);
  return UNITY_END();
}

// WARNING!!! PLEASE REMOVE UNNECESSARY MAIN IMPLEMENTATIONS //

/**
 * For native dev-platform or for some embedded frameworks
 */
int main(void)
{
  return runUnityTests();
}