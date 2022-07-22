#pragma once

#include"Logger.h"
#include"Scheduler.h"

enum class InterruptType	//todo: more interrupt sources!
{
	VBlank,
	HBlank,
	VCount,
	Serial,
	DMA0,
	DMA1,
	DMA2,
	DMA3,
	Timer0,
	Timer1,
	Timer2,
	Timer3,
	Keypad
};

class InterruptManager
{
public:
	InterruptManager(std::shared_ptr<Scheduler> scheduler);
	~InterruptManager();

	void requestInterrupt(InterruptType intType);
	bool getInterruptsEnabled() { return IME & 0b1; }
	bool getInterrupt();

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);
	static void eventHandler(void* context);
private:
	void onEvent();
	bool irqPending = false;
	std::shared_ptr<Scheduler> m_scheduler;
	uint16_t IE = {}, shadowIF = {};
	uint16_t IF = {};
	uint16_t IME = {};
};