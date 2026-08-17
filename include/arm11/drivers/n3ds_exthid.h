#pragma once

/*
 *   This file is part of libn3ds
 *   Copyright (C) 2026 open_agb_firm contributors
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "types.h"


#ifdef __cplusplus
extern "C"
{
#endif

enum
{
	N3DS_EXTHID_BUTTON_ZR = BIT(1),
	N3DS_EXTHID_BUTTON_ZL = BIT(2)
};

typedef struct
{
	u8 status;
	u8 buttons;
	u8 cstickX;
	u8 cstickY;
	u8 stopByte;
} N3dsExtHidSample;
static_assert(sizeof(N3dsExtHidSample) == 5, "Unexpected N3DS EXTHID sample size!");


/**
 * @brief      Initializes and starts the New 3DS external HID device.
 *
 * @return     Returns true when the device is available and sampling.
 */
bool N3DS_EXTHID_init(void);

/**
 * @brief      Sets the C-Stick digital direction threshold in corrected axis units.
 *
 * @param[in]  threshold  Exact press threshold in the range 0-255.
 */
void N3DS_EXTHID_setCstickDeadzone(u16 threshold);

/**
 * @brief      Stops sampling and puts the New 3DS external HID device to sleep.
 */
void N3DS_EXTHID_deinit(void);

/**
 * @brief      Reads a pending sample and returns the cached logical HID keys.
 *
 * @return     ZL, ZR and digital C-Stick direction bits from hid.h.
 */
u32 N3DS_EXTHID_scanInput(void);

/**
 * @brief      Converts one raw sample into logical HID keys.
 *
 * @param[in]  sample        Raw five-byte external HID sample.
 * @param[in]  previousKeys    Previous logical state, used for C-Stick hysteresis.
 * @param[in]  pressThreshold  Direction press threshold in corrected axis units.
 * @param[in]  releaseThreshold Direction release threshold in corrected axis units.
 *
 * @return     ZL, ZR and digital C-Stick direction bits from hid.h.
 */
u32 N3DS_EXTHID_decodeSample(const N3dsExtHidSample *sample, u32 previousKeys,
                             u16 pressThreshold, u16 releaseThreshold);

#ifdef __cplusplus
} // extern "C"
#endif
