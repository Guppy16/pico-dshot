#include "unity.h"
#include "packet.h"

void setUp(void)
{
  // set stuff up here
}

void tearDown(void)
{
  // clean stuff up here
}

void test_dshot_code_telemtry_to_cmd(void)
{
  // Code: 1046 = 0b 0100 0001 0110
  uint16_t code = 1046;
  uint16_t telemetry, expected_cmd, actual_cmd;

  telemetry = 0;
  expected_cmd = 0b100000101100;
  actual_cmd = dshot_code_telemtry_to_cmd(code, telemetry);
  TEST_ASSERT_EQUAL_MESSAGE(expected_cmd, actual_cmd, "Telemetry: 0");

  telemetry = 1;
  expected_cmd = 0b100000101101;
  actual_cmd = dshot_code_telemtry_to_cmd(code, telemetry);
  TEST_ASSERT_EQUAL_MESSAGE(expected_cmd, actual_cmd, "Telemetry: 1");
}

void test_dshot_cmd_crc(void)
{
  // CRC for 1046 + 0 telemetry = 0110
  // NOTE: 1046 = 0b10000010110
  uint16_t crc = dshot_cmd_crc(1046 << 1);
  TEST_ASSERT_EQUAL(0b0110, crc);
}

void test_dshot_cmd_to_frame(void)
{
  // Command: 1046 + 0 telemetry
  // CRC 0110
  // frame: 1046 + 0 + 0110
  uint16_t frame = dshot_cmd_to_frame(1046 << 1);
  TEST_ASSERT_EQUAL(0b1000001011000110, frame);
}

void test_dshot_frame_to_packet(void)
{
  // Frame is any 16 bit number
  uint16_t frame;
  uint32_t packet[16] = {0};
  uint32_t expected_packet[16];

  const dshot_packet_config conf_chn0 = {
      .dshot_high = 75, .dshot_low = 33, .pwm_channel = 0};

  // Test with frame = 0
  frame = 0b0;
  for (int i = 0; i < 16; ++i)
  {
    expected_packet[i] = conf_chn0.dshot_low;
  }
  dshot_frame_to_packet(frame, packet, conf_chn0);
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_packet, packet, 16, "frame = 0b0, Channel 0");

  // Test with frame = 1
  frame = 0b1;
  for (int i = 0; i < 16; ++i)
  {
    expected_packet[i] = conf_chn0.dshot_low;
  }
  expected_packet[16 - 1] = conf_chn0.dshot_high;
  dshot_frame_to_packet(frame, packet, conf_chn0);
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_packet, packet, 16, "frame = 0b1, Channel 0");

  // Test channel = 1, packet = 1
  const dshot_packet_config conf_chn1 = {
      .dshot_high = 75, .dshot_low = 33, .pwm_channel = 1};
  frame = 0b1;
  for (int i = 0; i < 16; ++i)
  {
    expected_packet[i] = conf_chn1.dshot_low;
  }
  expected_packet[16 - 1] = conf_chn1.dshot_high;
  for (int i = 0; i < 16; ++i)
  {
    expected_packet[i] <<= 16;
  }
  dshot_frame_to_packet(frame, packet, conf_chn1);
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_packet, packet, 16, "frame = 0b1, channel 1");
}

void test_dshot_cmd_to_packet(void)
{
  uint16_t frame;
  const dshot_packet_config conf = {
      .dshot_high = 75, .dshot_low = 33, .pwm_channel = 0};
  uint32_t buffer_size = 18;
  uint32_t packet[buffer_size] = {0};
  uint32_t expected_packet[buffer_size] = {0};

  // code: 1, telemetry: 1
  // Expected frame: 0x0033

  dshot_cmd_to_packet(1, 1, packet + 1, conf);

  for (int i = 1; i < buffer_size - 1; ++i)
  {
    expected_packet[i] = conf.dshot_low;
  }
  expected_packet[15] = expected_packet[16] = expected_packet[12] = expected_packet[11] = conf.dshot_high;
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_packet, packet, buffer_size, "Code = 1, Telemetry = 1, Channel 0, Array idx: 1");
}

int runUnityTests(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_dshot_code_telemtry_to_cmd);
  RUN_TEST(test_dshot_cmd_crc);
  RUN_TEST(test_dshot_cmd_to_frame);
  RUN_TEST(test_dshot_cmd_to_packet);
  RUN_TEST(test_dshot_frame_to_packet);
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