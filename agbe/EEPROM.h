#pragma once

#include"Logger.h"

#include<iostream>

class EEPROM
{
public:
	EEPROM();
	~EEPROM();

	//data width wouldn't matter bc we only care about the least significant bit of whatever's being sent
	uint8_t read(uint32_t address);
	void write(uint32_t address, uint8_t value);
private:
	uint64_t ROMData[1024];
	uint64_t requestData = 0;
	uint64_t dataToWrite = 0;
	uint16_t curAddress = 0;
	int requestProgress = 0;
	bool isRead = false;

	uint64_t readbackData = 0;
	int readbackProgress = 0;
};