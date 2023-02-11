#include <Arduino.h>
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "dshot.h"
#include "utils.h"

#define DSHOT_SPEED 150 // kHz
#define MCU_FREQ 120    // MHz

#define DEBUG 0

// Note that these should be cast uint32_t when sent to the slice
// WRAP = Total number of counts in PWM cycle
constexpr uint16_t DMA_WRAP = DEBUG ? (1 << 16) - 1 : 1000 * MCU_FREQ / DSHOT_SPEED;
constexpr uint16_t DSHOT_LOW = 0.37 * DMA_WRAP;
constexpr uint16_t DSHOT_HIGH = 0.75 * DMA_WRAP;

constexpr uint32_t cmd_nums = 1403;
constexpr uint32_t cmd_size = 20;
constexpr size_t dma_buffer_length = cmd_size * (cmd_nums + 2);
uint32_t dma_buffer[dma_buffer_length] = {0};

void code_to_dma_buffer(const uint16_t &code, const bool &telemtry, const uint &channel)
{
  size_t dma_buffer_idx = 0;
  size_t max_throttle = 200;

  // Keep throttle at 0
  for (size_t i = 0; i < 800; ++i)
  {
    Serial.print(0);
    Serial.print("\t");
    DShot::command_to_pwm_buffer(0, 0, dma_buffer + dma_buffer_idx, DSHOT_LOW, DSHOT_HIGH, channel);
    dma_buffer_idx += cmd_size;
  }

  // Increase throttle
  for (size_t i = 48; i < max_throttle; i += 4)
  {
    Serial.print(i);
    Serial.print("\t");
    DShot::command_to_pwm_buffer(i, 0, dma_buffer + dma_buffer_idx, DSHOT_LOW, DSHOT_HIGH, channel);
    dma_buffer_idx += cmd_size;
  }

  // Decrease throttle
  for (size_t i = max_throttle; i >= 48; i -= 4)
  {
    Serial.print(i);
    Serial.print("\t");
    DShot::command_to_pwm_buffer(i, 0, dma_buffer + dma_buffer_idx, DSHOT_LOW, DSHOT_HIGH, channel);
    dma_buffer_idx += cmd_size;
  }

  // Keep throttle at 0
  for (size_t i = 0; i < 5; ++i)
  {
    Serial.print(48);
    Serial.print("\t");
    DShot::command_to_pwm_buffer(48, 0, dma_buffer + dma_buffer_idx, DSHOT_LOW, DSHOT_HIGH, channel);
    dma_buffer_idx += cmd_size;
  }

  // while (dma_buffer_idx < dma_buffer_length)
  // {
  //   Serial.print(48);
  //   Serial.print("\t");
  //   DShot::command_to_pwm_buffer(48, 0, dma_buffer + dma_buffer_idx, DSHOT_LOW, DSHOT_HIGH, channel);
  //   dma_buffer_idx += cmd_size;
  // }

  Serial.println();
  Serial.print("DMA Buffer idx: ");
  Serial.println(dma_buffer_idx);

  // DShot::command_to_pwm_buffer(750, 0, dma_buffer + dma_buffer_idx, DSHOT_LOW, DSHOT_HIGH, channel);
}

int dma_chan = dma_claim_unused_channel(true);
dma_channel_config dma_conf = dma_channel_get_default_config(dma_chan);

constexpr uint MOTOR_GPIO = DEBUG ? 25 : 14;
uint pwm_slice_num = pwm_gpio_to_slice_num(MOTOR_GPIO);
uint pwm_channel = pwm_gpio_to_channel(MOTOR_GPIO);

// Setup a timer at 8 kHz
// Configure DMA channel to resend last command
// Currently it will just flash LED
int64_t alarm_callback(alarm_id_t id, void *user_data)
{
  dma_channel_configure(
      dma_chan,
      &dma_conf,
      &pwm_hw->slice[pwm_slice_num].cc, // Write to PWM counter compare
      dma_buffer,
      dma_buffer_length,
      true);

  // Switch LED state
}

void setup()
{
  Serial.begin(9600);
  // Flash LED
  utils::flash_led(LED_BUILTIN);

  // --- Setup PWM
  gpio_set_function(MOTOR_GPIO, GPIO_FUNC_PWM);

  // Without config method
  // NOTE: pwm should start after DMA initialised!
  // OR can initialise PWM with 0 channel level
  pwm_set_wrap(pwm_slice_num, DMA_WRAP);
  pwm_set_chan_level(pwm_slice_num, pwm_channel, 0);
  pwm_set_clkdiv(pwm_slice_num, DEBUG ? 240.0f : 1.0f); // Should run at 500 kHz for cpu-clck = 120 Mhz
  pwm_set_enabled(pwm_slice_num, true);

  // Use config method
  // pwm_config conf = pwm_get_default_config();
  // pwm_config_set_wrap(&conf, 3);
  // pwm_set_chan_level(pwm_slice_num, pwm_channel, 2);
  // pwm_init(pwm_slice_num, &conf, true);
  // pwm_set_chan_level(pwm_slice_num, pwm_channel, 2);

  // --- Setup DMA packet
  uint16_t code = 0;
  bool telemetry = 0;
  code_to_dma_buffer(code, telemetry, pwm_channel);

  // --- Setup DMA
  // PWM counter compare is 32 bit (16 bit per channel)
  channel_config_set_transfer_data_size(&dma_conf, DMA_SIZE_32);
  // Increment read address
  channel_config_set_read_increment(&dma_conf, true);
  // NO increment write address
  channel_config_set_write_increment(&dma_conf, false);
  // DMA Data request when PWM is finished
  channel_config_set_dreq(&dma_conf, DREQ_PWM_WRAP0 + pwm_slice_num);

  dma_channel_configure(
      dma_chan,
      &dma_conf,
      &pwm_hw->slice[pwm_slice_num].cc, // Write to PWM counter compare
      dma_buffer,
      dma_buffer_length,
      true);

  delay(2000);
  Serial.print("LED_BUILTIN GPIO: ");
  Serial.println(LED_BUILTIN);
  Serial.print("MOTOR GPIO: ");
  Serial.println(MOTOR_GPIO);
  Serial.print("PWM Slice num: ");
  Serial.println(pwm_slice_num);
  Serial.print("PWM Channel: ");
  Serial.println(pwm_channel);
  Serial.print("DMA channel: ");
  Serial.println(dma_chan);

  // Print DShot settings
  Serial.print("DShot: Wrap: ");
  Serial.print(DMA_WRAP);
  Serial.print(" Low: ");
  Serial.print(DSHOT_LOW);
  Serial.print(" High: ");
  Serial.println(DSHOT_HIGH);
  Serial.print("DMA Buffer Length: ");
  Serial.println(dma_buffer_length);

  // Print packet setup
  Serial.print("Packet Setup:");
  Serial.print(" Code: ");
  Serial.print(code, HEX);
  Serial.print(" Telemetry: ");
  Serial.print(telemetry);
  Serial.print(" Packet: ");
  Serial.print(DShot::command_to_packet(code << 1 | telemetry), HEX);
  Serial.println();

  // utils::print_conf(conf);
}
int incomingByte;
void loop()
{

  if (Serial.available() > 0)
  {
    incomingByte = Serial.read();

    // l (led)
    if (incomingByte == 108)
    {
      utils::flash_led(LED_BUILTIN);
    }

    // t (test)
    if (incomingByte == 116)
    {
      code_to_dma_buffer(0, 0, pwm_channel);
      delay(2000);
      dma_channel_configure(
          dma_chan,
          &dma_conf,
          &pwm_hw->slice[pwm_slice_num].cc, // Write to PWM counter compare
          dma_buffer,
          dma_buffer_length,
          true);
      delay(500);
      dma_channel_configure(
          dma_chan,
          &dma_conf,
          &pwm_hw->slice[pwm_slice_num].cc, // Write to PWM counter compare
          dma_buffer,
          dma_buffer_length,
          true);
    }

    // a (arm)
    if (incomingByte == 97)
    {

      code_to_dma_buffer(0, 0, pwm_channel);

      dma_channel_configure(
          dma_chan,
          &dma_conf,
          &pwm_hw->slice[pwm_slice_num].cc, // Write to PWM counter compare
          dma_buffer,
          dma_buffer_length,
          true);
    }

    // s (spin)
    if (incomingByte == 115)
    {
      // Reset DMA Buffer
      for (size_t i = 0; i < dma_buffer_length; ++i)
      {
        dma_buffer[i] = 0;
      }

      // Fill with increasing throttle value
      size_t dma_buffer_idx = 0;
      for (size_t i = 48; i < 55; ++i)
      {
        DShot::command_to_pwm_buffer(i, 0, dma_buffer + dma_buffer_idx, DSHOT_LOW, DSHOT_HIGH, pwm_channel);
        dma_buffer_idx += cmd_size;
      }

      // DShot::command_to_pwm_buffer(750, 0, dma_buffer, DSHOT_LOW, DSHOT_HIGH, pwm_channel);

      dma_channel_configure(
          dma_chan,
          &dma_conf,
          &pwm_hw->slice[pwm_slice_num].cc, // Write to PWM counter compare
          dma_buffer,
          dma_buffer_length,
          true);
    }

    // b - beep
    if (incomingByte == 98)
    {
      // Reset DMA Buffer
      for (size_t i = 0; i < dma_buffer_length; ++i)
      {
        dma_buffer[i] = 0;
      }

      DShot::command_to_pwm_buffer(1, 1, dma_buffer, DSHOT_LOW, DSHOT_HIGH, pwm_channel);

      dma_channel_configure(
          dma_chan,
          &dma_conf,
          &pwm_hw->slice[pwm_slice_num].cc, // Write to PWM counter compare
          dma_buffer,
          dma_buffer_length,
          true);
    }

    // spacebar - disarm?
    if (incomingByte == 32)
    {
      // Reset DMA Buffer
      for (size_t i = 0; i < dma_buffer_length; ++i)
      {
        dma_buffer[i] = 0;
      }

      DShot::command_to_pwm_buffer(0, 0, dma_buffer, DSHOT_LOW, DSHOT_HIGH, pwm_channel);

      dma_channel_configure(
          dma_chan,
          &dma_conf,
          &pwm_hw->slice[pwm_slice_num].cc, // Write to PWM counter compare
          dma_buffer,
          dma_buffer_length,
          true);
    }

    Serial.println(incomingByte, DEC);
  }

  // digitalWrite(LED_BUILTIN, HIGH);
  // delay(200);

  // digitalWrite(LED_BUILTIN, LOW);
  // delay(200);
}