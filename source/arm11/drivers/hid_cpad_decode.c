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

u32 hidDecodeCirclePadDirections(const CpadPos *const pos, const u32 previousKeys,
                                 const u16 pressThreshold, const u16 releaseThreshold)
{
	if(pos == NULL) return previousKeys & KEY_CPAD_MASK;

	u32 keys = 0;
	if(axisPositive(pos->x, (previousKeys & KEY_CPAD_RIGHT) != 0,
	                pressThreshold, releaseThreshold)) keys |= KEY_CPAD_RIGHT;
	if(axisNegative(pos->x, (previousKeys & KEY_CPAD_LEFT) != 0,
	                pressThreshold, releaseThreshold)) keys |= KEY_CPAD_LEFT;
	if(axisPositive(pos->y, (previousKeys & KEY_CPAD_UP) != 0,
	                pressThreshold, releaseThreshold)) keys |= KEY_CPAD_UP;
	if(axisNegative(pos->y, (previousKeys & KEY_CPAD_DOWN) != 0,
	                pressThreshold, releaseThreshold)) keys |= KEY_CPAD_DOWN;

	return keys;
}
