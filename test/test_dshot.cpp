#include "unity.h"
#include "dshot.h"

void setUp(void)
{
  // set stuff up here
}

void tearDown(void)
{
  // clean stuff up here
}

void test_command_to_crc(void)
{
  // CRC for 1046 + 0 telemetry = 0110
  // NOTE: 1046 = 0b10000010110
  uint16_t crc = DShot::command_to_crc(1046 << 1);
  TEST_ASSERT_EQUAL(0b0110, crc);
}

void test_command_to_packet(void)
{
  // Command: 1046 + 0 telemetry
  // CRC 0110
  // Packet: 1046 + 0 + 0110
  uint16_t packet = DShot::command_to_packet(1046 << 1);
  TEST_ASSERT_EQUAL(0b1000001011000110, packet);
}

void test_packet_to_pwm(void)
{
  // Packet is any 16 bit number
  uint16_t packet;
  uint32_t channel = 0;
  uint16_t DShot_low = 33;
  uint16_t DShot_hi = 75;
  uint32_t pwm_buffer[16] = {0};
  uint32_t expected_pwm_buffer[16];

  // Test with packet = 0
  packet = 0b0;
  for (int i = 0; i < 16; ++i){expected_pwm_buffer[i] = DShot_low;}
  DShot::packet_to_pwm(packet, pwm_buffer, DShot_low, DShot_hi, channel);
  // TEST_ASSERT_EQUAL_UINT32_ARRAY_
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_pwm_buffer, pwm_buffer, 16, "Packet = 0b0, Channel 0");
  // TEST_ASSERT_EQUAL(expected_pwm_buffer, pwm_buffer);

  // Test with packet = 1
  packet = 0b1;
  for (int i = 0; i < 16; ++i){expected_pwm_buffer[i] = DShot_low;}
  expected_pwm_buffer[16 - 1] = DShot_hi;
  DShot::packet_to_pwm(packet, pwm_buffer, DShot_low, DShot_hi, channel);
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_pwm_buffer, pwm_buffer, 16, "Packet = 0b1, Channel 0");

  // Test channel with packet = 1
  channel = 1;
  packet = 0b1;
  for (int i = 0; i < 16; ++i){expected_pwm_buffer[i] = DShot_low;}
  expected_pwm_buffer[16 - 1] = DShot_hi;
  for (int i = 0; i < 16; ++i){expected_pwm_buffer[i] <<= 16;}
  DShot::packet_to_pwm(packet, pwm_buffer, DShot_low, DShot_hi, channel);
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_pwm_buffer, pwm_buffer, 16, "Packet = 0b1, Channel 1");
}

/*
Test:
- command to crc
- command to packet
- packet to pwm
*/
void test_command_to_pwm(void)
{
  
}

int runUnityTests(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_command_to_crc);
  RUN_TEST(test_command_to_packet);
  RUN_TEST(test_packet_to_pwm);
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

// /**
//   * For Arduino framework
//   */
// void setup() {
//   // Wait ~2 seconds before the Unity test runner
//   // establishes connection with a board Serial interface
//   delay(2000);

//   runUnityTests();
// }
// void loop() {}

// /**
//   * For ESP-IDF framework
//   */
// void app_main() {
//   runUnityTests();
// }