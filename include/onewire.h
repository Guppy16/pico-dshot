/**
 * @file onewire.h
 * @defgroup onewire onewire
 * @brief
 *
 * Container to store results from telemetry receivied over one wire uart
 * telemetry
 * TODO: rename struct to onewire
 */

#pragma once
#include "dshot.h"
#include "hardware/uart.h"
#include "kissesctelem.h"
#include "stdint.h"

/**
 * @brief set the number of ESCs at compile time
 * This is used by the telemetry to tell how many ESCs to read the telemetry
 * from
 */
#ifndef ESC_COUNT
#define ESC_COUNT 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ONEWIRE_BAUDRATE 115200

/**
 * @brief minimum time interval between telemetry request
 *
 * NOTE: minium time interval between telem should be:
 * min_telem_interval = dshot_interval + telem_delay + telem_data_transfer
 *                    = 133 us (DSHOT150) + 0 (?)    + 115200 / 10*8 (=695 us)
 *                    = 827 us
 * Hence min_telem_interval ~ 1 ms is a safe value
 */
static const long int ONEWIRE_MIN_INTERVAL_US = 1000;

/**
 * @brief Struct to represent ESC data
 *
 */
typedef struct esc_motor {
  dshot_config *dshot;
  kissesc_telem_t telem_data;
} esc_motor_t;

/**
 * @brief struct to configue one wire uart telemetry
 *
 * @ingroup onewire
 * General config:
 * @param gpio NOTE: only one gpio because esc telem is one wire
 * @param req_flag Only request telemetry if req flag is true
 * @param send_req_rt Repeating timer config for preodically requesting
 * telemetry
 * @param buffer_size = 10 KISS telemetry protocol outputs 10 bytes
 * Storing uart telemetry output
 * @param buffer_idx Keep track of idx in uart telemetry buffer
 * @param buffer Array to hold telemetry buffer data
 */
typedef struct telem_uart {
  uart_inst_t *uart;
  uint gpio;
  uint baudrate;                 /// Defaults to @ref ONEWIRE_BAUDRATE
  bool req_flag;                 // Only request telemetry if req_flag is true
  repeating_timer_t send_req_rt; // Repeating timer config for periodically
                                 // requesting telemetry
  bool send_req_rt_state;        // Technically this is redundant
  volatile size_t esc_motor_idx; // Keep track of which esc to request telem
  volatile esc_motor_t escs[ESC_COUNT];

  // variable to store the result from a telemetry read in an irq
  volatile size_t buffer_idx;
  volatile uint8_t buffer[KISS_ESC_TELEM_BUFFER_SIZE];
} onewire_t;

// Global variable for onewire
onewire_t onewire;

/**
 * @brief IRQ for reading telemetry data over uart
 *
 * When onewire is receiving data over uart, an interrupt will be raised.
 * This routine is called as an interrupt service routine.
 * This routine stores the telemtry data in onewire->buffer.
 * Once the buffer is full, the buffer is parsed to telemtry data
 * and stored in the relevant ESC's telem_data data store
 *
 * NOTE: we assume that the uart is automatically cleared in hw
 */
static void onewire_uart_irq(void) {
  // Read uart greedily
  while (uart_is_readable(onewire.uart)) {
    // Check if reached end of buffer
    if (onewire.buffer_idx >= KISS_ESC_TELEM_BUFFER_SIZE) {
      printf("Detected overflow in onewire buffer\n");
      /// TODO: panic?
    } else {
      const char c = uart_getc(onewire.uart);
      onewire.buffer[onewire.buffer_idx] = (uint8_t)c;
      // printf("%x\t", c);
    }
    onewire.buffer_idx++;
    // printf("UART: %x\n", onewire.buffer[onewire.buffer_idx - 1]);
  }
  if (onewire.buffer_idx == KISS_ESC_TELEM_BUFFER_SIZE) {
    // Convert buffer to telemetry data and populate the relevant ESC
    kissesc_buffer_to_telem(onewire.buffer,
                            &onewire.escs[onewire.esc_motor_idx].telem_data);
  }
}

/**
 * @brief Validation checks, else return false
 * This function should be called before sending a uart telemetry request (e.g.
 * in @ref onewire_repeating_req)
 *
 * 1. All telemtry bits in ESCs should be unset
 * 2. telem->buffer_idx == KISS_ESC_TELEM_BUFFER_SIZE
 *
 * @param telem onewire_t
 * @return true  if validation checks pass
 * @return false if validation checks fail
 */
static inline bool validate_onewire_repeating_req(onewire_t *const telem) {
  // Check if telemetry bit set in any ESC
  uint telemetry_bit_set = 0;
  for (size_t i = 0; i < ESC_COUNT; ++i) {
    telemetry_bit_set += telem->escs[i].dshot->packet.telemetry;
  }
  if (telemetry_bit_set)
    return false;

  // Check if we have recieved all telemetry data
  if (telem->buffer_idx != KISS_ESC_TELEM_BUFFER_SIZE)
    return false;

  return true;
}

/**
 * @brief Function for repeatedly request telemetry
 *
 * Set the telemetry bit in the ESCs in a round-robin fashion.
 * @attention This assumes that dshot_send_packet resets the telemetry bit after
 * sending the packet. We also assume that this routine will not interrupt
 * dshot_send_packet (which I believe is true if we use the same alarm pool,
 * because alarms on the same pool have the same priority, hence don't interrupt
 * each other)
 *
 * @param rt
 * @return `telem->req_flag` (set this to false to stop requesting telemetry)
 */
static inline bool onewire_repeating_req(repeating_timer_t *rt) {
  onewire_t *telem = (onewire_t *)(rt->user_data);

  if (!validate_onewire_repeating_req(telem)) {
    printf("Onewire validation failed. Stopped requesting telemetry");
    for (size_t i = 0; i < ESC_COUNT; ++i) {
      telem->escs[i].dshot->packet.telemetry = 0;
    }
    telem->req_flag = false;
    return false;
  }

  // --- Reset data:
  // Reset telemetry bit if not done so
  telem->escs[telem->esc_motor_idx].dshot->packet.telemetry = 0;
  // Reset buffer_idx
  telem->buffer_idx = 0;
  // No need to reset onewire->buffer, because it will be overwritten anyways

  // Configure the next ESC to reqest telemetry over uart
  telem->esc_motor_idx = (telem->esc_motor_idx + 1) % ESC_COUNT;
  telem->escs[telem->esc_motor_idx].dshot->packet.telemetry = 1;

  return telem->req_flag;
}

/**
 * @brief Configure uart and gpio
 *
 * @param telem uart_telem object
 * @param pull_up
 */
static inline void onewire_uart_gpio_setup(onewire_t *const telem,
                                           bool pull_up) {
  // Initialise uart pico and set baudrate
  telem->baudrate = uart_init(telem->uart, ONEWIRE_BAUDRATE);
  // Turn off flow control
  uart_set_hw_flow(telem->uart, false, false);
  /// TODO: set data format explicitly
  // uart_set_format(telem->uart, 8, 1, UART_PARITY_NONE);
  /// TODO: check if FIFO affects the speed of interpreting chars
  // uart_set_fifo_enabled(telem->uart, false);

  // Set GPIO pin mux for UART RX
  /// TODO: assert that this pin is meant to be used for UART RX
  gpio_set_function(telem->gpio, GPIO_FUNC_UART);
  // Optionally set pull up resistor
  gpio_pull_up(telem->gpio);
}

/**
 * @brief Setup IRQ handler for onewire uart on RX
 *
 * @param telem
 */
static void onewire_setup_irq(onewire_t *const telem, irq_handler_t handler) {
  // Add exclusive interrupt handler on RX (for parsing onewire telemetry)
  const int UART_IRQ = telem->uart == uart0 ? UART0_IRQ : UART1_IRQ;
  irq_set_exclusive_handler(UART_IRQ, handler);
  irq_set_enabled(UART_IRQ, true);
  // Enable uart interrupt on RX
  uart_set_irq_enables(telem->uart, true, false);
}

/**
 * @brief initialise uart telemetry config
 *
 * @param gpio GPIO port connected to onewire telemetry uart
 * @param req_flag Request telemetry on initialisation
 * @param pool alarm pool to add the repeating timer to request telemetry
 * regularly
 * @param dshot array of dshot configs. The pointers are stored so that
 * telemetry bit can be set when required
 */
static void telem_uart_init(onewire_t *const telem, uart_inst_t *uart,
                            uint gpio, alarm_pool_t *const pool,
                            long int telem_interval,
                            dshot_config *escs[ESC_COUNT], bool req_flag,
                            bool pull_up) {
  // Initialise uart and gpio
  telem->uart = uart;
  telem->gpio = gpio;
  onewire_uart_gpio_setup(telem, pull_up);

  // copy pointers to ESC configs
  for (size_t esc_num = 0; esc_num < ESC_COUNT; ++esc_num) {
    telem->escs[esc_num].dshot = escs[esc_num];
  }
  telem->esc_motor_idx = 0;
  telem->buffer_idx = KISS_ESC_TELEM_BUFFER_SIZE;
  for (size_t i = 0; i < KISS_ESC_TELEM_BUFFER_SIZE; ++i) {
    telem->buffer[i] = 0;
  }

  // Setup onewire uart IRQ to handle telemetry data
  onewire_setup_irq(telem, onewire_uart_irq);

  // Setup telemetry repeating timer
  telem_interval = MAX(ONEWIRE_MIN_INTERVAL_US * ESC_COUNT, telem_interval);
  telem->req_flag = req_flag;
  telem->send_req_rt_state = alarm_pool_add_repeating_timer_us(
      pool, telem_interval, onewire_repeating_req, telem, &telem->send_req_rt);
}

/// @brief print uart_telem config
void print_onewire_config(onewire_t *onewire);

#ifdef __cplusplus
}
#endif