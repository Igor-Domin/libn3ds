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
#include "arm11/drivers/n3ds_exthid.h"
#include "arm11/drivers/hid.h"
#include "arm11/drivers/i2c.h"
#include "arm11/drivers/gpio.h"
#include "arm11/drivers/interrupt.h"
#include "arm11/drivers/mcu.h"
#include "arm11/drivers/timer.h"


#define EXTHID_SAMPLE_PERIOD_MS  (16u)
#define EXTHID_INIT_ATTEMPTS     (2u)


static atomic_bool g_sampleReady = false;
static bool g_initialized = false;
static bool g_enabled = false;
static bool g_irqRegistered = false;
static bool g_deviceSelected = false;
static u8 g_deviceRevision = 0;
static u32 g_keys = 0;
static u16 g_cstickPressThreshold = 64;
static u16 g_cstickReleaseThreshold = 38;



static bool isNewModel(void)
{
	switch(MCU_getSystemModel())
	{
		case SYS_MODEL_N3DS:
		case SYS_MODEL_N3DS_XL:
		case SYS_MODEL_N2DS_XL:
			return true;
		default:
			return false;
	}
}

static bool sendCommand(const u8 command)
{
	return I2C_write(I2C_DEV_EXTHID, I2C_NO_REG_VAL, command);
}

static bool readSample(void *const out)
{
	return I2C_readArray(I2C_DEV_EXTHID, I2C_NO_REG_VAL, out, sizeof(N3dsExtHidSample));
}

static bool sleepDevice(void)
{
	if(g_deviceRevision == 2 && !sendCommand(0x37)) return false;
	if(!sendCommand(0xF7)) return false; // Disable output.

	TIMER_sleepMs(6);
	N3dsExtHidSample unused;
	if(!readSample(&unused)) return false;

	if(!sendCommand(0x7C)) return false;
	return sendCommand(0xD0); // Sleep with auto-calibration.
}

static void wakeChip(void)
{
	GPIO_write(GPIO_EXTHID_WAKE, 0);
	TIMER_sleepUs(100);
	GPIO_write(GPIO_EXTHID_WAKE, 1);
	TIMER_sleepMs(10);
}

static bool wakeDevice(void)
{
	wakeChip();
	if(!sendCommand(0x7F)) return false;

	TIMER_sleepMs(136); // Wait for calibration.
	if(!sendCommand(0xF9)) return false; // Enable output.
	if(g_deviceRevision == 2 && !sendCommand(0x33)) return false;

	return true;
}

static bool initializeDevice(void)
{
	GPIO_config(GPIO_EXTHID_WAKE, GPIO_OUTPUT);
	GPIO_write(GPIO_EXTHID_WAKE, 1);

	if(!sendCommand(0xC1)) return false; // Select device mode.
	g_deviceSelected = true;
	if(!sendCommand(0x52)) return false; // Select conversion mode.
	if(!sendCommand(0xF7)) return false; // Disable output.

	typedef struct
	{
		u8 status;
		u8 vendor;
		u8 revision;
		u8 reserved;
		u8 stopByte;
	} ExtHidDeviceId;
	static_assert(sizeof(ExtHidDeviceId) == 5, "Unexpected N3DS EXTHID device ID size!");

	ExtHidDeviceId deviceId;
	if(!sendCommand(0xF5)) return false;
	TIMER_sleepMs(2);
	if(!readSample(&deviceId)) return false;
	if(deviceId.status != 0x82 || deviceId.stopByte != 0xFF) return false;
	g_deviceRevision = deviceId.revision;
	g_initialized = true;

	// Nintendo's IR module applies this configuration only to vendor 1.
	if(deviceId.vendor == 1 && g_deviceRevision >= 1)
	{
		if(!sendCommand(0x08)) return false; // Sensitivity 1.0.
		if(!sendCommand(0xF3)) return false; // Disable idle state.
		if(!sendCommand(0xF2)) return false; // Automatic calibration with threshold.
		if(!sendCommand(0x29)) return false; // Auto-calibration threshold.
		if(!sendCommand(0x84)) return false; // Operation threshold 5.
		if(!sendCommand(0x96)) return false; // Output threshold 7.
		if(!sendCommand(0xB4)) return false; // Operation timer.
		if(!sendCommand(0x63)) return false; // Output timer.
	}

	if(deviceId.vendor == 1 && g_deviceRevision >= 2 && !sendCommand(0x33)) return false;
	return sleepDevice();
}

static bool setSamplePeriod(const u32 periodMs)
{
	if(periodMs < 10 || periodMs > 21) return false;
	if(!sendCommand(0xA0u | ((periodMs - 10) & 0xFu))) return false;
	TIMER_sleepMs(56);
	return true;
}

static void extHidIrqHandler(UNUSED const u32 intSource)
{
	atomic_store_explicit(&g_sampleReady, true, memory_order_relaxed);
}

static bool stopSampling(void)
{
	// Keep data-ready IRQs active until output is disabled and the device sleeps.
	const bool slept = !g_deviceSelected || sleepDevice();
	GPIO_config(GPIO_EXTHID_IRQ, GPIO_INPUT);
	if(g_irqRegistered)
	{
		IRQ_unregisterIsr(IRQ_EXTHID);
		g_irqRegistered = false;
	}
	atomic_store_explicit(&g_sampleReady, false, memory_order_relaxed);
	g_enabled = false;
	// Preserve the possible-active state when a transport failure prevents a
	// confirmed sleep, so a later init/deinit call can retry the cleanup.
	g_deviceSelected = !slept;
	g_keys = 0;
	return slept;
}

static bool startSampling(void)
{
	IRQ_registerIsr(IRQ_EXTHID, 13, 0, extHidIrqHandler);
	g_irqRegistered = true;
	GPIO_config(GPIO_EXTHID_IRQ, GPIO_IRQ_FALLING | GPIO_INPUT);

	if(!wakeDevice() || !setSamplePeriod(EXTHID_SAMPLE_PERIOD_MS))
	{
		(void)stopSampling();
		return false;
	}

	atomic_store_explicit(&g_sampleReady, true, memory_order_relaxed);
	g_enabled = true;
	return true;
}

bool N3DS_EXTHID_init(void)
{
	if(g_enabled) return true;

	// Never probe the device on old models: the absent I2C target incurs retries.
	if(!isNewModel()) return false;
	if(g_initialized || g_irqRegistered || g_deviceSelected)
	{
		(void)stopSampling();
		g_initialized = false;
	}

	for(u32 attempt = 0; attempt < EXTHID_INIT_ATTEMPTS; attempt++)
	{
		g_deviceRevision = 0;
		if(initializeDevice() && startSampling()) return true;

		// This also attempts to sleep a device reached before a later command or
		// sample read failed, rather than leaving it partially awake.
		(void)stopSampling();
		g_initialized = false;
		if(attempt + 1 < EXTHID_INIT_ATTEMPTS) TIMER_sleepMs(2);
	}

	return false;
}

void N3DS_EXTHID_deinit(void)
{
	if(!g_initialized && !g_irqRegistered && !g_deviceSelected) return;
	(void)stopSampling();
	g_initialized = false;
}

void N3DS_EXTHID_setCstickDeadzone(u16 threshold)
{
	if(threshold > 255) threshold = 255;
	g_cstickPressThreshold = threshold;
	g_cstickReleaseThreshold = (threshold * 3u + 2u) / 5u;
	g_keys &= ~KEY_CSTICK_MASK;
}

u32 N3DS_EXTHID_scanInput(void)
{
	if(!g_enabled) return 0;
	if(!atomic_exchange_explicit(&g_sampleReady, false, memory_order_relaxed)) return g_keys;

	N3dsExtHidSample sample;
	if(!readSample(&sample))
	{
		// A failed transfer must not synthesize a release. If data-ready is still
		// asserted, retry on the next scan because no new falling edge may arrive.
		if(GPIO_read(GPIO_EXTHID_IRQ) == 0)
			atomic_store_explicit(&g_sampleReady, true, memory_order_relaxed);
		return g_keys;
	}

	g_keys = N3DS_EXTHID_decodeSample(&sample, g_keys, g_cstickPressThreshold,
	                                  g_cstickReleaseThreshold);
	// A new report can assert data-ready while this transfer is in progress.
	// Requeue it because a continuously-low line will not produce another edge.
	if(GPIO_read(GPIO_EXTHID_IRQ) == 0)
		atomic_store_explicit(&g_sampleReady, true, memory_order_relaxed);
	return g_keys;
}
