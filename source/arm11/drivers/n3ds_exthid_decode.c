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
#include "arm11/drivers/hid.h"
#include "arm11/drivers/n3ds_exthid.h"


static bool axisPositive(const s16 value, const bool wasHeld,
                         const u16 pressThreshold, const u16 releaseThreshold)
{
	const u16 threshold = (wasHeld ? releaseThreshold : pressThreshold);
	return value > 0 && (u16)value >= threshold;
}

static bool axisNegative(const s16 value, const bool wasHeld,
                         const u16 pressThreshold, const u16 releaseThreshold)
{
	const u16 threshold = (wasHeld ? releaseThreshold : pressThreshold);
	return value < 0 && (u16)-value >= threshold;
}

u32 N3DS_EXTHID_decodeSample(const N3dsExtHidSample *const sample, const u32 previousKeys,
                             const u16 pressThreshold, const u16 releaseThreshold)
{
	if(sample == NULL || (sample->status & 0xFCu) != 0x80u || sample->stopByte != 0xFFu)
		return previousKeys;

	u32 keys = 0;
	if(sample->buttons & N3DS_EXTHID_BUTTON_ZL) keys |= KEY_ZL;
	if(sample->buttons & N3DS_EXTHID_BUTTON_ZR) keys |= KEY_ZR;

	// The device axes are rotated 45 degrees. This is the inverse of the transform
	// used by Nintendo's IR input format, with the common sqrt(2) scale omitted.
	const s16 rawX = (s16)sample->cstickX - 0x80;
	const s16 rawY = (s16)sample->cstickY - 0x80;
	const s16 x = rawX - rawY;
	const s16 y = rawX + rawY;

	if(axisPositive(x, (previousKeys & KEY_CSTICK_RIGHT) != 0,
	                pressThreshold, releaseThreshold)) keys |= KEY_CSTICK_RIGHT;
	if(axisNegative(x, (previousKeys & KEY_CSTICK_LEFT) != 0,
	                pressThreshold, releaseThreshold)) keys |= KEY_CSTICK_LEFT;
	if(axisPositive(y, (previousKeys & KEY_CSTICK_UP) != 0,
	                pressThreshold, releaseThreshold)) keys |= KEY_CSTICK_UP;
	if(axisNegative(y, (previousKeys & KEY_CSTICK_DOWN) != 0,
	                pressThreshold, releaseThreshold)) keys |= KEY_CSTICK_DOWN;

	return keys;
}
