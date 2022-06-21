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
	uint64_t currentTimestamp = m_scheduler->getCurrentTimestamp();
	for (int i = 0; i < 4; i++)
	{
		uint8_t ctrlreg = m_timers[i].CNT_H;
		bool timerEnabled = (ctrlreg >> 7) & 0b1;
		if (timerEnabled)
		{
			uint64_t timerOverflowTime = m_timers[i].overflowTime;
			if (timerOverflowTime <= currentTimestamp)
			{
				//overflowed, need to handle
				m_timers[i].initialClock = m_timers[i].CNT_L;
				m_timers[i].clock = m_timers[i].initialClock;
				calculateNextOverflow(i, timerOverflowTime,false);	//don't update with our current clock, because we might have overshot the overflow time slightly.

				bool doIrq = (ctrlreg >> 6) & 0b1;
				if (doIrq)
				{
					InterruptType irqLUT[4] = { InterruptType::Timer0,InterruptType::Timer1,InterruptType::Timer2,InterruptType::Timer3 };
					m_interruptManager->requestInterrupt(irqLUT[i]);
				}

			}
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
		setCurrentClock(timerIdx, m_timers[timerIdx].CNT_H&0b11);
		return m_timers[timerIdx].clock & 0xFF;
	case 1:
		setCurrentClock(timerIdx,m_timers[timerIdx].CNT_H&0b11);
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
		bool timerWasEnabled = (m_timers[timerIdx].CNT_H >> 7) & 0b1;
		bool timerNowEnabled = (value >> 7) & 0b1;
		bool countup = ((value >> 2) & 0b1);
		uint8_t oldPrescalerSetting = m_timers[timerIdx].CNT_H & 0b11;
		uint8_t newPrescalerSetting = value & 0b11;
		m_timers[timerIdx].CNT_H &= 0xFF00; m_timers[timerIdx].CNT_H |= value;

		if (!timerWasEnabled && timerNowEnabled && !countup)
		{
			m_timers[timerIdx].initialClock = m_timers[timerIdx].CNT_L;
			m_timers[timerIdx].clock = m_timers[timerIdx].initialClock;
			calculateNextOverflow(timerIdx, m_scheduler->getCurrentTimestamp(),true);
		}

		if (timerWasEnabled && timerNowEnabled && (newPrescalerSetting != oldPrescalerSetting))	//prescaler has changed!!
		{
			setCurrentClock(timerIdx, oldPrescalerSetting);	//update clock with old prescaler
			calculateNextOverflow(timerIdx, m_scheduler->getCurrentTimestamp(),false);	//then find next overflow time
		}

		break;
	}
}

void Timer::calculateNextOverflow(int timerIdx, uint64_t timeBase, bool first)
{
	uint8_t timerctrl = m_timers[timerIdx].CNT_H;

	uint64_t cycleLut[4] = { 1, 64, 256, 1024 };
	uint8_t prescalerSelect = (timerctrl & 0b11);
	uint64_t prescalerCycles = cycleLut[prescalerSelect];

	uint64_t cyclesToOverflow = (65536 - m_timers[timerIdx].clock) * prescalerCycles;

	uint64_t currentTime = timeBase;
	if (first)
		currentTime++;
	uint64_t overflowTimestamp = currentTime + cyclesToOverflow;

	Event timerEventLUT[4] = { Event::TIMER0,Event::TIMER1,Event::TIMER2,Event::TIMER3 };
	m_scheduler->addEvent(timerEventLUT[timerIdx], &Timer::onSchedulerEvent, (void*)this, overflowTimestamp);

	m_timers[timerIdx].timeActivated = currentTime;
	m_timers[timerIdx].lastUpdateTimestamp = currentTime;
	m_timers[timerIdx].overflowTime = overflowTimestamp;

}

void Timer::setCurrentClock(int idx, uint8_t prescalerSetting)
{
	uint8_t timerctrl = m_timers[idx].CNT_H;

	bool timerEnabled = ((timerctrl >> 7) & 0b1);
	bool countup = ((timerctrl >> 2) & 0b1);
	if (!timerEnabled || countup)	//don't predict new clock if disabled, or timer is a countup timer
		return;

	uint64_t currentTime = m_scheduler->getCurrentTimestamp();
	uint64_t timeSinceLastCheck = m_timers[idx].lastUpdateTimestamp;

	uint64_t cycleLut[4] = { 1, 64, 256, 1024 };
	uint64_t prescalerCycles = cycleLut[prescalerSetting];

	uint64_t ticksElapsed = (currentTime - timeSinceLastCheck) / prescalerCycles;
	if (ticksElapsed > 0)
	{
		m_timers[idx].clock += ticksElapsed;
		m_timers[idx].lastUpdateTimestamp += (ticksElapsed * prescalerCycles);
	}
}

void Timer::onSchedulerEvent(void* context)
{
	Timer* thisPtr = (Timer*)context;
	thisPtr->event();
}