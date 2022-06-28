#pragma once

#include"Logger.h"
#include"Scheduler.h"
#include"InterruptManager.h"

#include<iostream>

//Only purpose of this is to fake serial - i.e. generate an irq at the correct time :)

class SerialStub
{
public:
	SerialStub(std::shared_ptr<Scheduler> scheduler, std::shared_ptr<InterruptManager> interruptManager);
	~SerialStub();

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t data);

	static void eventCallback(void* context);
private:
	std::shared_ptr<Scheduler> m_scheduler;
	std::shared_ptr<InterruptManager> m_interruptManager;

	void serialEvent();
	void calculateNextEvent();
	bool eventInProgress = false;

	uint16_t SIOCNT = {};
	uint8_t SIODATA8 = {};
	uint32_t SIODATA32 = {};
};