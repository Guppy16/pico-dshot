/*
 *  This file builds off of tts
 *  to send dshot frames
 *  with a repeating timer
 */

#pragma once
#include "tts.h"

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

    /*! \brief sends a dshot frame with optional debug info
     */
    void send_dshot_frame(bool debug = false);

    // Repeating timer setup
}