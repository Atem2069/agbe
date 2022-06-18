#pragma once

#include"Logger.h"

#include<iostream>

struct AudioFIFO
{
	int8_t data[32];
	int startIdx = 0;
	int endIdx = 0;
	int size = 0;

	void push(int8_t val)
	{
		data[endIdx] = val;
		endIdx = (endIdx + 1) & 0x1F;	//limit range to 0->31
		size++;
	}
	int8_t pop()
	{
		int8_t retVal = data[startIdx];
		startIdx = (startIdx + 1) & 0x1F;
		size--;
		return retVal;
	}
	void empty()
	{
		startIdx = 0; endIdx = 0;
	}

};

class APU
{
public:
	APU();
	~APU();

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);
private:
	AudioFIFO m_channels[2];

	uint16_t SOUNDCNT_H = {};
	uint8_t SOUNDCNT_X = {};
	uint16_t SOUNDBIAS = {};

};