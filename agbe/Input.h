#pragma once

#include<iostream>
#include"Logger.h"

struct InputState
{
	bool A;
	bool B;
	bool Select;
	bool Start;
	bool Right;
	bool Left;
	bool Up;
	bool Down;
	bool L;
	bool R;
};

class Input
{
public:
	Input();
	~Input();

	void update(InputState newInput);

	uint8_t readIORegister(uint32_t address);
	void writeIORegister(uint32_t address, uint8_t value);

private:
	uint16_t keyInput = 0;
};