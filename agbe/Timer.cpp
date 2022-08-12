#include"Timer.h"

Timer::Timer(std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<Scheduler> scheduler)
{
	m_scheduler = scheduler;
	m_interruptManager = interruptManager;
	for (int i = 0; i < 4; i++)	//clear timer io registers
		m_timers[i] = {};
}

Timer::~Timer()
{

}

void Timer::registerAPUCallbacks(callbackFn timer0, callbackFn timer1, void* ctx)
{
	apuOverflowCallbacks[0] = timer0;
	apuOverflowCallbacks[1] = timer1;
	apuCtx = ctx;
}

void Timer::event()
{
	uint64_t currentTimestamp = m_scheduler->getEventTime();

	int timerIdx = 0;
	switch (m_scheduler->getLastFiredEvent())
	{
	case Event::TIMER0:
		break;
	case Event::TIMER1:
		timerIdx = 1;
		break;
	case Event::TIMER2:
		timerIdx = 2;
		break;
	case Event::TIMER3:
		timerIdx = 3;
		break;
	}

	uint8_t ctrlreg = m_timers[timerIdx].CNT_H;
	bool timerEnabled = (ctrlreg >> 7) & 0b1;
	bool cascade = (ctrlreg >> 2) & 0b1;
	uint64_t timerOverflowTime = m_timers[timerIdx].overflowTime;
	//overflowed, need to handle
	m_timers[timerIdx].initialClock = m_timers[timerIdx].CNT_L;
	m_timers[timerIdx].clock = m_timers[timerIdx].initialClock;
	calculateNextOverflow(timerIdx, timerOverflowTime);	//don't update with our current clock, because we might have overshot the overflow time slightly.
	setCurrentClock(timerIdx, m_timers[timerIdx].CNT_H & 0b11, m_scheduler->getCurrentTimestamp());
	checkCascade(timerIdx + 1);
	bool doIrq = (ctrlreg >> 6) & 0b1;
	if (doIrq)
		m_interruptManager->requestInterrupt(irqLUT[timerIdx]);

	if ((timerIdx == 0 || timerIdx == 1) && timerEnabled && !cascade)
		apuOverflowCallbacks[timerIdx](apuCtx);
}

uint8_t Timer::readIO(uint32_t address)
{
	uint32_t timerIdx = ((address - 0x4000100) / 4);	//4000100-4000103 = timer 0, etc.
	uint32_t addrOffset = ((address - 0x4000100) & 3);	//figure out which byte we're writing (0-3)

	setCurrentClock(timerIdx, m_timers[timerIdx].CNT_H & 0b11,m_scheduler->getCurrentTimestamp());

	bool cascade = (m_timers[timerIdx].CNT_H >> 2) & 0b1;
	if (cascade)
		m_scheduler->tick();	//hehe - the aging cart cascade test requires quite tight timing, so force all pending events to fire first 
								//some pending timer events may occur too late otherwise, which means we fail :(
	switch (addrOffset)
	{
	case 0:
		return m_timers[timerIdx].clock & 0xFF;
	case 1:
		return (m_timers[timerIdx].clock >> 8) & 0xFF;
	case 2:
		return m_timers[timerIdx].CNT_H & 0xFF;
	case 3:
		return (m_timers[timerIdx].CNT_H >> 8) & 0xFF;
	}
}

void Timer::writeIO(uint32_t address, uint8_t value)
{
	uint32_t timerIdx = ((address - 0x4000100) / 4);	//4000100-4000103 = timer 0, etc.
	uint32_t addrOffset = ((address - 0x4000100) & 3);	//figure out which byte we're writing (0-3)

	switch (addrOffset)
	{
	case 0:
		//m_timers[timerIdx].CNT_L &= 0xFF00; m_timers[timerIdx].CNT_L |= value;
		m_timers[timerIdx].newReloadVal &= 0xFF00; m_timers[timerIdx].newReloadVal |= value;
		m_timers[timerIdx].newReloadWritten = true;
		m_scheduler->addEvent(Event::TimerRegWrite, &Timer::onReloadRegWrite, (void*)this, m_scheduler->getCurrentTimestamp() + 1);
		break;
	case 1:
		m_timers[timerIdx].newReloadVal &= 0xFF;  m_timers[timerIdx].newReloadVal |= (value << 8);
		if (!m_timers[timerIdx].newReloadWritten)	//if for some reason only the top byte of the reload is written? 
		{
			m_timers[timerIdx].newReloadWritten = true;
			m_scheduler->addEvent(Event::TimerRegWrite, &Timer::onReloadRegWrite, (void*)this, m_scheduler->getCurrentTimestamp() + 1);
		}
		break;
	case 2:
		m_timers[timerIdx].newControlWritten = true;
		m_timers[timerIdx].newControlVal = value;
		m_scheduler->addEvent(Event::TimerRegWrite, &Timer::onControlRegWrite, (void*)this, m_scheduler->getCurrentTimestamp() + 1);
		break;
	}
}

void Timer::calculateNextOverflow(int timerIdx, uint64_t timeBase)
{
	uint8_t timerctrl = m_timers[timerIdx].CNT_H;

	static constexpr uint64_t cycleLut[4] = { 1, 64, 256, 1024 };
	static constexpr uint64_t shiftLut[4] = { 0,6,8,10 };
	static constexpr uint64_t unsetMask[4] = { 0xFFFFFFFF,0xFFFFFFC0,0xFFFFFF00,0xFFFFFC00 };
	uint8_t prescalerSelect = (timerctrl & 0b11);
	uint64_t prescalerCycles = cycleLut[prescalerSelect];

	uint64_t cyclesToOverflow = (65535 - m_timers[timerIdx].clock) * prescalerCycles;

	//this bit is magic :)
	uint32_t nextPrescalerEdge = timeBase&0xFFFF;
	if (shiftLut[prescalerSelect] != 0)
	{
		uint16_t addMask = 0b1 << (shiftLut[prescalerSelect]);
		nextPrescalerEdge += addMask;								//essentially finding the prescaler cycle where the next falling edge occurs
		nextPrescalerEdge &= unsetMask[prescalerSelect];
		cyclesToOverflow += (nextPrescalerEdge - (timeBase&0xFFFF));	//then finding out how many cycles until it happens
	}
	else
		cyclesToOverflow++;


	uint64_t currentTime = timeBase;
	uint64_t overflowTimestamp = currentTime + cyclesToOverflow;

	m_scheduler->removeEvent(timerEventLUT[timerIdx]);	//just in case :)
	m_scheduler->addEvent(timerEventLUT[timerIdx], &Timer::onSchedulerEvent, (void*)this, overflowTimestamp);

	m_timers[timerIdx].timeActivated = currentTime;
	m_timers[timerIdx].lastUpdateClock = (currentTime >> shiftLut[prescalerSelect]);
	m_timers[timerIdx].overflowTime = overflowTimestamp;

}

void Timer::checkCascade(int timerIdx)
{
	if (timerIdx > 3)
		return;
	uint8_t timerctrl = m_timers[timerIdx].CNT_H;
	bool timerEnabled = ((timerctrl >> 7) & 0b1);
	bool isCascade = ((timerctrl >> 2) & 0b1);
	if (!isCascade)
		return;

	if (m_timers[timerIdx].clock == 0xFFFF)
	{
		m_timers[timerIdx].clock = m_timers[timerIdx].CNT_L;
		bool doIrq = (timerctrl >> 6) & 0b1;
		if (doIrq)
			m_interruptManager->requestInterrupt(irqLUT[timerIdx]);
		checkCascade(timerIdx + 1);
	}
	else
		m_timers[timerIdx].clock++;
}

void Timer::setCurrentClock(int idx, uint8_t prescalerSetting, uint64_t timestamp)
{
	uint8_t timerctrl = m_timers[idx].CNT_H;

	bool timerEnabled = ((timerctrl >> 7) & 0b1);
	bool countup = ((timerctrl >> 2) & 0b1);
	if (!timerEnabled || countup)	//don't predict new clock if disabled, or timer is a countup timer
		return;

	static constexpr uint64_t cycleLut[4] = { 1, 64, 256, 1024 };
	static constexpr uint64_t shiftLut[4] = { 0,6,8,10 };
	uint64_t currentClock = (timestamp >> shiftLut[prescalerSetting]);
	uint64_t timeDiff = currentClock - m_timers[idx].lastUpdateClock;
	m_timers[idx].clock += timeDiff;
	m_timers[idx].lastUpdateClock = currentClock;
}

void Timer::writeControl()
{
	for (int timerIdx = 0; timerIdx < 4; timerIdx++)
	{
		if (!m_timers[timerIdx].newControlWritten)
			continue;
		m_timers[timerIdx].newControlWritten = false;
		setCurrentClock(timerIdx, m_timers[timerIdx].CNT_H & 0b11,m_scheduler->getEventTime());				//update clock first if possible
		bool timerWasEnabled = (m_timers[timerIdx].CNT_H >> 7) & 0b1;
		bool timerNowEnabled = (m_timers[timerIdx].newControlVal >> 7) & 0b1;
		bool countup = ((m_timers[timerIdx].newControlVal >> 2) & 0b1);
		bool wasCountup = (m_timers[timerIdx].CNT_H >> 2) & 0b1;
		uint8_t oldPrescalerSetting = m_timers[timerIdx].CNT_H & 0b11;
		uint8_t newPrescalerSetting = m_timers[timerIdx].newControlVal & 0b11;

		m_timers[timerIdx].CNT_H = m_timers[timerIdx].newControlVal;

		if (timerIdx == 0)
		{
			m_timers[timerIdx].CNT_H &= 0b11111011;	//clear cascade bit on timer0
			countup = false;
		}

		if (!timerWasEnabled && timerNowEnabled)
		{
			m_timers[timerIdx].clock = m_timers[timerIdx].CNT_L;	//load in reload value
			if (!countup)
			{
				m_timers[timerIdx].initialClock = m_timers[timerIdx].clock;
				calculateNextOverflow(timerIdx, m_scheduler->getEventTime()+1);		//+1 to account for 2-cycle startup delay
			}
		}
		if (timerWasEnabled && timerNowEnabled)
			calculateNextOverflow(timerIdx, m_scheduler->getEventTime());

		if ((timerWasEnabled && !timerNowEnabled) || (countup))			//if timer becomes disabled, or it becomes a countup timer: unschedule
			m_scheduler->removeEvent(timerEventLUT[timerIdx]);
	}
}

void Timer::writeReload()
{
	for (int timerIdx = 0; timerIdx < 4; timerIdx++)
	{
		if (!m_timers[timerIdx].newReloadWritten)
			continue;
		m_timers[timerIdx].newReloadWritten = false;
		m_timers[timerIdx].CNT_L=m_timers[timerIdx].newReloadVal;
	}
}

void Timer::onSchedulerEvent(void* context)
{
	Timer* thisPtr = (Timer*)context;
	thisPtr->event();
}

void Timer::onControlRegWrite(void* context)
{
	Timer* thisPtr = (Timer*)context;
	thisPtr->writeControl();
}

void Timer::onReloadRegWrite(void* context)
{
	Timer* thisPtr = (Timer*)context;
	thisPtr->writeReload();
}