#pragma once

#include"Logger.h"
#include"InterruptManager.h"

struct TimerRegister
{
	uint16_t CNT_L;
	uint16_t CNT_H;
	uint16_t clock;
	int ticksSinceLastClock = 0;
};

class Timer
{
public:
	Timer(std::shared_ptr<InterruptManager> interruptManager);
	~Timer();

	void step();

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);
private:
	TimerRegister m_timers[4];
	std::shared_ptr<InterruptManager> m_interruptManager;
};