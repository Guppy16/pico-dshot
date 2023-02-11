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

void test_dshot_code_telemtry_to_command(void){
  // Code: 1046 = 0b 0100 0001 0110
  uint16_t code = 1046;
  uint16_t telemetry, expected_cmd, actual_cmd;

  telemetry = 0;
  expected_cmd = 0b100000101100;
  actual_cmd = DShot::dshot_code_telemtry_to_command(code, telemetry);
  TEST_ASSERT_EQUAL_MESSAGE(expected_cmd, actual_cmd, "Telemetry: 0");

  telemetry = 1;
  expected_cmd = 0b100000101101;
  actual_cmd = DShot::dshot_code_telemtry_to_command(code, telemetry);
  TEST_ASSERT_EQUAL_MESSAGE(expected_cmd, actual_cmd, "Telemetry: 1");


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
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_pwm_buffer, pwm_buffer, 16, "Packet = 0b0, Channel 0");

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
  uint16_t packet;
  uint32_t channel = 0;
  uint16_t DShot_low = 33;
  uint16_t DShot_hi = 75;
  uint32_t pwm_buffer[18] = {0};
  uint32_t expected_pwm_buffer[18] = {0};

  // Test normal packet with non standard sized array
  // Packet 0b1, start idx: 1
  packet = 0b1;
  for (int i = 1; i < 18 - 1; ++i){expected_pwm_buffer[i] = DShot_low;}
  expected_pwm_buffer[16] = DShot_hi;
  DShot::packet_to_pwm(packet, pwm_buffer + 1, DShot_low, DShot_hi, channel);
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_pwm_buffer, pwm_buffer, 18, "Packet = 0b1, Channel 0, Array idx: 1");  

  // Test same but with packet set as so:
  // Command: 1, Telemetry: 1
  // Expected Packet: 0x0033
  // Expected PWM
  for (int i = 1; i < 18 - 1; ++i){expected_pwm_buffer[i] = DShot_low;}
  expected_pwm_buffer[15] = expected_pwm_buffer[16]
  = expected_pwm_buffer[12] = expected_pwm_buffer[11] = DShot_hi;
  packet = DShot::command_to_packet(DShot::dshot_code_telemtry_to_command(1, 1));
  DShot::packet_to_pwm(packet, pwm_buffer + 1, DShot_low, DShot_hi, channel);
  TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected_pwm_buffer, pwm_buffer, 18, "Packet = 0x0033, Channel 0, Array idx: 1");  
}

int runUnityTests(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_dshot_code_telemtry_to_command);
  RUN_TEST(test_command_to_crc);
  RUN_TEST(test_command_to_packet);
  RUN_TEST(test_packet_to_pwm);
  RUN_TEST(test_command_to_pwm);
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