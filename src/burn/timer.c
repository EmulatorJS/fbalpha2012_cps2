// Timers (for Yamaha FM cips and generic)
#include "burnint.h"
#include "timer.h"
#include "m68000_intf.h"
#include "z80_intf.h"

#define MAX_TIMER_VALUE ((1 << 30) - 65536)

static INT32 nTimerCount[2], nTimerStart[2];

// Callbacks
static INT32 (*pTimerOverCallback)(INT32, INT32);
static double (*pTimerTimeCallback)();

static INT32 nCPUClockspeed = 0;
static INT32 (*pCPUTotalCycles)() = NULL;
static INT32 (*pCPURun)(INT32) = NULL;
static void (*pCPURunEnd)() = NULL;

// ---------------------------------------------------------------------------
// Running time

static double BurnTimerTimeCallbackDummy(void)
{
	return 0.0;
}

// ---------------------------------------------------------------------------
// Update timers

static INT32 nTicksTotal, nTicksDone, nTicksExtra;

INT32 BurnTimerUpdate(INT32 nCycles)
{
	INT32 nIRQStatus = 0;

	nTicksTotal = MAKE_TIMER_TICKS(nCycles, nCPUClockspeed);

	while (nTicksDone < nTicksTotal)
	{
		INT32 nTimer, nCyclesSegment, nTicksSegment;
		// Determine which timer fires first
		if (nTimerCount[0] <= nTimerCount[1])
			nTicksSegment = nTimerCount[0];
		else
			nTicksSegment = nTimerCount[1];
		if (nTicksSegment > nTicksTotal)
			nTicksSegment = nTicksTotal;

		nCyclesSegment = MAKE_CPU_CYCLES(nTicksSegment + nTicksExtra, nCPUClockspeed);

		pCPURun(nCyclesSegment - pCPUTotalCycles());

		nTicksDone = MAKE_TIMER_TICKS(pCPUTotalCycles() + 1, nCPUClockspeed) - 1;

		nTimer = 0;
		if (nTicksDone >= nTimerCount[0]) {
			if (nTimerStart[0] == MAX_TIMER_VALUE)
				nTimerCount[0] = MAX_TIMER_VALUE;
			else
				nTimerCount[0] += nTimerStart[0];
			nTimer |= 1;
		}
		if (nTicksDone >= nTimerCount[1]) {
			if (nTimerStart[1] == MAX_TIMER_VALUE)
				nTimerCount[1] = MAX_TIMER_VALUE;
			else 
				nTimerCount[1] += nTimerStart[1];
			nTimer |= 2;
		}
		if (nTimer & 1) {
			nIRQStatus |= pTimerOverCallback(0, 0);
		}
		if (nTimer & 2) {
			nIRQStatus |= pTimerOverCallback(0, 1);
		}
	}

	return nIRQStatus;
}

void BurnTimerEndFrame(INT32 nCycles)
{
	INT32 nTicks = MAKE_TIMER_TICKS(nCycles, nCPUClockspeed);

	BurnTimerUpdate(nCycles);

	if (nTimerCount[0] < MAX_TIMER_VALUE)
		nTimerCount[0] -= nTicks;
	if (nTimerCount[1] < MAX_TIMER_VALUE)
		nTimerCount[1] -= nTicks;
	nTicksDone -= nTicks;
	if (nTicksDone < 0)
		nTicksDone = 0;
}

// ---------------------------------------------------------------------------
// Callbacks for the sound cores

void BurnTimerSetRetrig(INT32 c, double period)
{
	pCPURunEnd();

	if (period == 0.0) {
		nTimerStart[c] = nTimerCount[c] = MAX_TIMER_VALUE;
		return;
	}

	nTimerStart[c]  = nTimerCount[c] = (INT32)(period * (double)(TIMER_TICKS_PER_SECOND));
	nTimerCount[c] += MAKE_TIMER_TICKS(pCPUTotalCycles(), nCPUClockspeed);
}

// ------------------------------------ ---------------------------------------
// Initialisation etc.
void BurnTimerExit(void)
{
	nCPUClockspeed = 0;
	pCPUTotalCycles = NULL;
	pCPURun = NULL;
	pCPURunEnd = NULL;

	return;
}

void BurnTimerReset(void)
{
	nTimerCount[0] = nTimerCount[1] = MAX_TIMER_VALUE;
	nTimerStart[0] = nTimerStart[1] = MAX_TIMER_VALUE;

	nTicksDone = 0;
}

INT32 BurnTimerInit(INT32 (*pOverCallback)(INT32, INT32), double (*pTimeCallback)())
{
	BurnTimerExit();

	pTimerOverCallback = pOverCallback;
	pTimerTimeCallback = pTimeCallback ? pTimeCallback : BurnTimerTimeCallbackDummy;

	BurnTimerReset();

	return 0;
}

INT32 BurnTimerAttachZet(INT32 nClockspeed)
{
	nCPUClockspeed  = nClockspeed;
	pCPUTotalCycles = ZetTotalCycles;
	pCPURun         = ZetRun;
	pCPURunEnd      = ZetRunEnd;

	nTicksExtra     = MAKE_TIMER_TICKS(1, nCPUClockspeed) - 1;

	return 0;
}
