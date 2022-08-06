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
	uint64_t lastUpdateClock;

	uint16_t newControlVal;
	bool newControlWritten = false;

	uint16_t newReloadVal;
	bool newReloadWritten = false;
};

class Timer
{
public:
	Timer(std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<Scheduler> scheduler);
	void registerAPUCallbacks(callbackFn timer0, callbackFn timer1, void* ctx);
	~Timer();

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);

	static void onSchedulerEvent(void* context);
	static void onControlRegWrite(void* context);
	static void onReloadRegWrite(void* context);

private:
	void event();
	void calculateNextOverflow(int timerIdx, uint64_t timeBase, bool first);
	void checkCascade(int timerIdx);
	void setCurrentClock(int idx, uint8_t prescalerSetting, uint64_t timestamp);

	void writeControl();
	void writeReload();
	TimerRegister m_timers[4];

	callbackFn apuOverflowCallbacks[2];
	void* apuCtx;

	std::shared_ptr<InterruptManager> m_interruptManager;
	std::shared_ptr<Scheduler> m_scheduler;
};