/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>

#include <platform.h>

#include "common/maths.h"

#include "build/build_config.h"

#include "config/parameter_group.h"
#include "fc/runtime_config.h"

#include "drivers/dma.h"
#include "drivers/system.h"

#include "drivers/serial.h"
#include "drivers/serial_uart.h"
#include "io/serial.h"

#include "rx/rx.h"
#include "rx/msp.h"

static uint16_t mspFrame[MAX_SUPPORTED_RC_CHANNEL_COUNT];
static bool rxMspFrameDone = false;

static uint32_t mspOffsetUpdateAt = 0;
static bool mspOffsetFrameReady = false;
static int16_t mspOffsetFrame[4]; // interval [-500;500]

static uint16_t rxMspReadRawRC(const rxRuntimeConfig_t *rxRuntimeConfigPtr, uint8_t chan)
{
    UNUSED(rxRuntimeConfigPtr);
    return mspFrame[chan];
}

void rxMspFrameReceive(uint16_t *frame, int channelCount)
{
    for (int i = 0; i < channelCount; i++) {
        mspFrame[i] = frame[i];
    }

    // Any channels not provided will be reset to zero
    for (int i = channelCount; i < MAX_SUPPORTED_RC_CHANNEL_COUNT; i++) {
        mspFrame[i] = 0;
    }

    rxMspFrameDone = true;
}

uint8_t rxMspFrameStatus(void)
{
    if (!rxMspFrameDone) {
        return RX_FRAME_PENDING;
    }

    rxMspFrameDone = false;
    return RX_FRAME_COMPLETE;
}

void rxMspInit(const rxConfig_t *rxConfig, rxRuntimeConfig_t *rxRuntimeConfig)
{
    UNUSED(rxConfig);

    rxRuntimeConfig->channelCount = MAX_SUPPORTED_RC_CHANNEL_COUNT;
    rxRuntimeConfig->rxRefreshRate = 20000;

    rxRuntimeConfig->rcReadRawFn = rxMspReadRawRC;
    rxRuntimeConfig->rcFrameStatusFn = rxMspFrameStatus;
}


// SINTEF JAKOB - Is this the best place to put this?
void rxMspOffsetFrameReceive(int16_t *frame, int channelCount)
{
    for (int i = 0; i < channelCount; i++) {
    	mspOffsetFrame[i] = constrain(frame[i],-500,500);
    }

    // Any channels not provided will be reset to zero
    for (int i = channelCount; i < 4; i++) {
    	mspOffsetFrame[i] = 0;
    }

    mspOffsetUpdateAt = millis();
    mspOffsetFrameReady = true;
}

int16_t rxMspReadOffsetRC(rxRuntimeConfig_t *rxRuntimeConfigPtr, uint8_t chan)
{
    UNUSED(rxRuntimeConfigPtr);

    if (!FLIGHT_MODE(RCOFFSETS_MODE))
    	return 0;

    if (chan >= 4)
    	return 0;
    if (!mspOffsetFrameReady)
    	return 0;
    if (millis() - mspOffsetUpdateAt > 100) {
    	mspOffsetFrameReady = false;
    	return 0;
    }

    return mspOffsetFrame[chan];
}
