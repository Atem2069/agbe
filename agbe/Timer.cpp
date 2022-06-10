#include"Timer.h"

Timer::Timer(std::shared_ptr<InterruptManager> interruptManager)
{
	m_interruptManager = interruptManager;
	for (int i = 0; i < 4; i++)	//clear timer io registers
		m_timers[i] = {};
}

Timer::~Timer()
{

}

void Timer::step()
{
	for (int i = 0; i < 4; i++)
	{
		bool timerEnabled = ((m_timers[i].CNT_H >> 7) & 0b1);
		if (!timerEnabled)
			continue;
		if ((m_timers[i].CNT_H >> 2) & 0b1)
			std::cout << "cascade not supported!!" << '\n';

		m_timers[i].ticksSinceLastClock++;

		int tickThreshold = 1;
		uint8_t timerPrescaler = ((m_timers[i].CNT_H & 0b11));
		switch (timerPrescaler)
		{
		case 1:
			tickThreshold = 64; break;
		case 2:
			tickThreshold = 256; break;
		case 3:
			tickThreshold = 1024; break;
		}

		if (m_timers[i].ticksSinceLastClock >= tickThreshold)
		{
			m_timers[i].ticksSinceLastClock = 0;	//hmm, if prescaler changes while running what happens? might mess this up
			if (m_timers[i].clock == 65535)	//overflow
			{
				bool doIrq = ((m_timers[i].CNT_H >> 6) & 0b1);
				if (doIrq)
				{
					InterruptType intLut[4] = { InterruptType::Timer0,InterruptType::Timer1,InterruptType::Timer2,InterruptType::Timer3 };
					m_interruptManager->requestInterrupt(intLut[i]);
				}
				m_timers[i].clock = m_timers[i].CNT_L;	//load in reload value
			}
			else
				m_timers[i].clock++;
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
		bool timerWasEnabled = ((m_timers[timerIdx].CNT_H) >> 7) & 0b1;
		bool newTimerEnabled = ((value >> 7) & 0b1);
		if (!timerWasEnabled && newTimerEnabled)				//reload timer value if timer is going to be enabled
			m_timers[timerIdx].clock = m_timers[timerIdx].CNT_L;
		m_timers[timerIdx].CNT_H &= 0xFF00; m_timers[timerIdx].CNT_H |= value;
		break;
	}
}