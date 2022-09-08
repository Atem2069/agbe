#pragma once

#include"Logger.h"
#include"Scheduler.h"

enum class InterruptType	//todo: more interrupt sources!
{
	VBlank=1,
	HBlank=2,
	VCount=4,
	Timer0=8,
	Timer1=16,
	Timer2=32,
	Timer3=64,
	Serial=128,
	DMA0=256,
	DMA1=512,
	DMA2=1024,
	DMA3=2048,
	Keypad=4096
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
	void checkIRQs();
	bool irqAvailable = false;
	bool pendingIrq = false;
	std::shared_ptr<Scheduler> m_scheduler;
	uint16_t IE = {};
	uint16_t IF = {};
	uint16_t IME = {};
};