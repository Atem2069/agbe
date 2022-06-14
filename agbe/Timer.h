#pragma once

#include"Logger.h"
#include"InterruptManager.h"
#include"Scheduler.h"

struct TimerRegister
{
	uint16_t CNT_L;
	uint16_t CNT_H;
	uint16_t initialClock;
	uint16_t clock;
	uint64_t timeActivated;
	uint64_t overflowTime;
};

class Timer
{
public:
	Timer(std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<Scheduler> scheduler);
	~Timer();

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);

	static void onSchedulerEvent(void* context);

private:
	void event();
	void calculateNextOverflow(int idx);
	void setCurrentClock(int idx);
	TimerRegister m_timers[4];
	std::shared_ptr<InterruptManager> m_interruptManager;
	std::shared_ptr<Scheduler> m_scheduler;
};