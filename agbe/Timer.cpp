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

void Timer::event()
{
	uint64_t curTimestamp = m_scheduler->getCurrentTimestamp();
	for (int i = 0; i < 4; i++)
	{
		bool timerEnabled = ((m_timers[i].CNT_H >> 7) & 0b1);
		if (!timerEnabled)
			continue;

		if (m_timers[i].overflowTime <= curTimestamp)
		{
			bool doIrq = ((m_timers[i].CNT_H >> 6) & 0b1);
			if (doIrq)
			{
				InterruptType intLut[4] = { InterruptType::Timer0,InterruptType::Timer1,InterruptType::Timer2,InterruptType::Timer3 };
				m_interruptManager->requestInterrupt(intLut[i]);
			}
			m_timers[i].clock = m_timers[i].CNT_L;
			calculateNextOverflow(i);
		}
	}
}

uint8_t Timer::readIO(uint32_t address)
{
	uint32_t timerIdx = ((address - 0x4000100) / 4);	//4000100-4000103 = timer 0, etc.
	uint32_t addrOffset = ((address - 0x4000100) % 4);	//figure out which byte we're writing (0-3)
	switch (addrOffset)
	{
	case 0:
		setCurrentClock(timerIdx);
		return m_timers[timerIdx].clock & 0xFF;
	case 1:
		setCurrentClock(timerIdx);
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
	uint32_t addrOffset = ((address - 0x4000100) % 4);	//figure out which byte we're writing (0-3)

	switch (addrOffset)
	{
	case 0:
		m_timers[timerIdx].CNT_L &= 0xFF00; m_timers[timerIdx].CNT_L |= value;
		break;
	case 1:
		m_timers[timerIdx].CNT_L &= 0xFF; m_timers[timerIdx].CNT_L |= ((value << 8));
		break;
	case 2:
		uint16_t oldControlBits = m_timers[timerIdx].CNT_H;
		bool timerWasEnabled = ((m_timers[timerIdx].CNT_H) >> 7) & 0b1;
		bool newTimerEnabled = ((value >> 7) & 0b1);

		uint8_t newPrescalerBits = value & 0b11;
		uint8_t timerPrescaleBits = oldControlBits & 0b11;
		if (timerWasEnabled && !newTimerEnabled)
			setCurrentClock(timerIdx);
		if (timerWasEnabled && newTimerEnabled && (newPrescalerBits != timerPrescaleBits))
		{
			setCurrentClock(timerIdx);
			m_timers[timerIdx].timeActivated = m_scheduler->getCurrentTimestamp();
			m_timers[timerIdx].CNT_H &= 0xFF00; m_timers[timerIdx].CNT_H |= value;
			calculateNextOverflow(timerIdx);
		}

		m_timers[timerIdx].CNT_H &= 0xFF00; m_timers[timerIdx].CNT_H |= value;
		if (!timerWasEnabled && newTimerEnabled)				//reload timer value if timer is going to be enabled
		{
			m_timers[timerIdx].clock = m_timers[timerIdx].CNT_L;
			calculateNextOverflow(timerIdx);
		}
		break;
	}
}

void Timer::calculateNextOverflow(int timerIdx)
{
	bool timerEnabled = (m_timers[timerIdx].CNT_H >> 7) & 0b1;
	if (!timerEnabled)
		return;
	m_timers[timerIdx].initialClock = m_timers[timerIdx].clock;
	uint64_t tickThreshold = 1;
	uint8_t timerPrescaler = ((m_timers[timerIdx].CNT_H & 0b11));
	switch (timerPrescaler)
	{
	case 1:
		tickThreshold = 64; break;
	case 2:
		tickThreshold = 256; break;
	case 3:
		tickThreshold = 1024; break;
	}
	uint64_t ticksTillOverflow = (65536 - m_timers[timerIdx].clock) * tickThreshold;

	Event timerEventType = Event::TIMER0;
	switch (timerIdx)
	{
	case 1: timerEventType = Event::TIMER1; break;
	case 2: timerEventType = Event::TIMER2; break;
	case 3:timerEventType = Event::TIMER3; break;
	}

	m_scheduler->addEvent(timerEventType, &Timer::onSchedulerEvent, (void*)this, m_scheduler->getCurrentTimestamp() + ticksTillOverflow+1);
	m_timers[timerIdx].timeActivated = m_scheduler->getCurrentTimestamp()+1;
	m_timers[timerIdx].overflowTime = m_timers[timerIdx].timeActivated + ticksTillOverflow;
}

void Timer::setCurrentClock(int idx)
{
	uint64_t tickThreshold = 1;
	uint8_t timerPrescaler = ((m_timers[idx].CNT_H & 0b11));
	switch (timerPrescaler)
	{
	case 1:
		tickThreshold = 64; break;
	case 2:
		tickThreshold = 256; break;
	case 3:
		tickThreshold = 1024; break;
	}

	uint64_t timerStart = m_timers[idx].timeActivated;
	uint64_t curTime = m_scheduler->getCurrentTimestamp();
	uint64_t differenceInClocks = curTime - timerStart;
	differenceInClocks /= tickThreshold;

	m_timers[idx].clock = m_timers[idx].initialClock + differenceInClocks;
}

void Timer::onSchedulerEvent(void* context)
{
	Timer* thisPtr = (Timer*)context;
	thisPtr->event();
}