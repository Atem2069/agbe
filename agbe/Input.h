#pragma once

#include<iostream>
#include"Logger.h"
#include"Scheduler.h"
#include"InterruptManager.h"

union InputState
{
	uint16_t reg;
	struct
	{
		bool A : 1;
		bool B : 1;
		bool Select : 1;
		bool Start : 1;
		bool Right : 1;
		bool Left : 1;
		bool Up : 1;
		bool Down : 1;
		bool R : 1;
		bool L : 1;
		bool unused10 : 1;
		bool unused11 : 1;
		bool unused12 : 1;
		bool unused13 : 1;
		bool unused14 : 1;
		bool unused15 : 1;
	};
};

class Input
{
public:
	Input();
	~Input();

	void registerInput(std::shared_ptr<InputState> inputState);
	void registerInterrupts(std::shared_ptr<InterruptManager> interruptManager);

	uint8_t readIORegister(uint32_t address);
	void writeIORegister(uint32_t address, uint8_t value);
	void tick();
	bool getIRQConditionsMet();
private:
	void checkIRQ();

	std::shared_ptr<InputState> m_inputState;
	std::shared_ptr<InterruptManager> m_interruptManager;
	uint64_t lastEventTime = 0;
	uint16_t keyInput = 0;
	uint16_t KEYCNT = 0;
	bool irqActive = false;
};