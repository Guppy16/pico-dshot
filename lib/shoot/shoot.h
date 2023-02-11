/*
 *  This file builds off of tts
 *  to send dshot frames
 *  with a repeating timer
 */

#pragma once
#include "tts.h"

#include "pico/time.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

namespace shoot
{
    // TODO: Does this ever need to be volatile?
    extern uint32_t dma_buffer[DSHOT_FRAME_LENGTH];
    extern uint32_t temp_dma_buffer[DSHOT_FRAME_LENGTH];

    // Declare throttle and telemetry variables
    // These are made global for now so that it is easy to modify
    extern uint16_t throttle_code;
    extern uint16_t telemetry;

    extern uint16_t writes_to_temp_dma_buffer;
    extern uint16_t writes_to_dma_buffer;

    /**
     *  \brief sends a dshot frame with optional debug info.
     *  converts the throttle_code and telemetry to a dshot frame
     *  writes to the frame buffer
     *  re-configures dma
     *
     *  \note A (premature?) optimisation is done by checking
     *  if the dma channel is busy, then calculate the frame
     *  and write to temp dma buffer.
     *  This is transferred to the normal dma_buffer afterwards
     *
     *  \param debug Used to print throttle code to serial
     */
    void send_dshot_frame(bool debug = false);

    // Repeating timer setup

    // Keep track of repeating timer status
    extern bool dma_alarm_rt_state;

    // Setup a repeating timer configuration
    extern struct repeating_timer send_frame_rt;

    // Create alarm pool
    extern alarm_pool_t *pico_alarm_pool;

    // Routine to setup repeating timer to send dshot frame
    void rt_setup();

    // ISR to send DShot frame over DMA
    bool repeating_send_dshot_frame(struct repeating_timer *rt);

}