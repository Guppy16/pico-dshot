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

// Buffer size of telemetry is 10 bytes for KISS ESC protocol
static const size_t ONEWIRE_BUFFER_SIZE = 10;
static const uint ONEWIRE_BAUDRATE = 115200;

/**
 * @brief minimum time interval between telemetry request
 *
 * NOTE: minium time interval between telem should be:
 * min_telem_interval = dshot_interval + telem_delay + telem_data_transfer
 *                    = 133 us (DSHOT150) + 0 (?)    + 115200 / 10*8 (=695 us)
 *                    = 827 us
 * Hence min_telem_interval ~ 1 ms is a safe value
 */
static const uint ONEWIRE_MIN_INTERVAL_US = 1000;

/**
 * @brief Struct to represent ESC data
 *
 */
typedef struct esc_motor {
  dshot_config *dshot;
  volatile char buffer[ONEWIRE_BUFFER_SIZE] = {0}; // Buffer to telemetry data
  /// TODO: store the telemetry data here (e.g. voltage, temp, etc)
} esc_motor;

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
  uint baudrate = ONEWIRE_BAUDRATE;
  bool req_flag;                 // Only request telemetry if req_flag is true
  repeating_timer_t send_req_rt; // Repeating timer config for periodically
                                 // requesting telemetry
  bool send_req_rt_state;        // Technically this is redundant
  volatile size_t esc_motor_idx = 0; // Keep track of which esc to request telem
  volatile esc_motor escs[ESC_COUNT];

  // variable to store the result from a telemetry read in an irq
  volatile size_t buffer_idx = 0;
  volatile char buffer[ONEWIRE_BUFFER_SIZE] = {0};

  // const uint buffer_size = 10;   // KISS telemetry protocol outputs 10 bytes

  /// --- Storing uart telemetry output
  // volatile size_t buffer_idx = 0; // Keep track of idx in uart telemetry
  // buffer volatile char buffer[buffer_size] = {0};
  /// --- Keep track of a list of pointers to dshot_config

} telem_uart;

// Global variable for onewire
telem_uart onewire;

/**
 * @brief IRQ for reading telemetry data over uart
 *
 * When onewire received data over uart, an interrupt will be raised, which will
 * call this routine. This will store the telemetry data in onewire->buffer and
 * (TODO:) parse it into telemetry information after all the data has been
 * received.
 *
 * NOTE: we assume that the uart is automatically cleared in hw
 */
void onewire_uart_irq(void) {
  // Read uart greedily
  while (uart_is_readable(onewire.uart)) {
    // Check if reached end of buffer
    if (onewire.buffer_idx == ONEWIRE_BUFFER_SIZE) {
      printf("Detected overflow in onewire buffer\n");
    } else {
      onewire.buffer[onewire.buffer_idx++] = uart_getc(onewire.uart);
    }
    // printf("UART: %x\n", onewire.buffer[onewire.buffer_idx - 1]);
  }
  if (onewire.buffer_idx == ONEWIRE_BUFFER_SIZE) {
    /// TODO: CRC check and populate the relevant ESC telem data
  }
}

/**
 * @brief Function for repeatedly request telemetry
 *
 * Set the telemetry bit in the ESCs in a round-robin fashion.
 * @attention This assumes that dshot_send_packet resets the telemetry bit after
 * sending the packet. We also assume that this routine will not interrupt
 * dshot_send_packet (which I believe is held if we use the same alarm pool as
 * the priority of this function will be the same)
 *
 * @param rt
 * @return `telem->req_flag` (set this to false to stop requesting telemetry)
 */
static inline bool onewire_repeating_req(repeating_timer_t *rt) {
  telem_uart *telem = (telem_uart *)(rt->user_data);
  /// TODO: cycle through escs to request telem
  /// TODO: some kinda validation to see if the prev telem request was
  // succesfully parsed. Possible checks include: buffer_idx == BUFFER_SIZE,
  // esc[idx] telemetry bit is not set. NOTE that the first pass of this
  // function may not be initialised for this validation check(!)

  // --- Reset data:
  // Reset telemetry bit if not done so
  telem->escs[telem->esc_motor_idx].dshot->packet.telemetry = 0;
  // Reset buffer_idx
  telem->buffer_idx = 0;
  // No need to reset onewire->buffer, because it will be overwritten anyways

  // Configure the next ESC to reqest telemetry over uart
  telem->escs[telem->esc_motor_idx++].dshot->packet.telemetry = 1;

  return telem->req_flag;
}

/**
 * @brief Configure uart and gpio
 *
 * @param telem uart_telem object
 * @param pull_up
 */
static inline void onewire_uart_gpio_setup(telem_uart *const telem,
                                           bool pull_up = false) {
  // Initialise uart pico and set baudrate
  telem->baudrate = uart_init(telem->uart, telem->baudrate);
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
static void onewire_setup_irq(telem_uart *const telem, irq_handler_t handler) {
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
 *
 */
static void telem_uart_init(telem_uart *const telem, uart_inst_t *uart,
                            uint gpio, alarm_pool_t *const pool,
                            long int telem_interval,
                            dshot_config escs[ESC_COUNT], bool req_flag = true,
                            bool pull_up = false
) {
  // Initialise uart and gpio
  telem->uart = uart;
  telem->gpio = gpio;
  onewire_uart_gpio_setup(telem, pull_up);

  // setup pointers to ESC configs
  for (size_t esc_num = 0; esc_num < ESC_COUNT; ++esc_num) {
    telem->escs[esc_num].dshot = &escs[esc_num];
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
void print_onewire_config(telem_uart *onewire);

#ifdef __cplusplus
}
#endif