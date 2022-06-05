#pragma once

#include"Logger.h"

enum class InterruptType	//todo: more interrupt sources!
{
	VBlank,
	HBlank,
	VCount
};

class InterruptManager
{
public:
	InterruptManager();
	~InterruptManager();

	void requestInterrupt(InterruptType intType);
	bool getInterrupt();

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);
private:
	uint16_t IE = {};
	uint16_t IF = {};
	uint16_t IME = {};
};