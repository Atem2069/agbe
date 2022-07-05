#pragma once

#include"Logger.h"
#include"BackupBase.h"
#include<iostream>

enum class WriteState
{
	RequestType,
	Address,
	Data
};

class EEPROM : public BackupBase
{
public:
	EEPROM(BackupType type);
	~EEPROM();

	//data width wouldn't matter bc we only care about the least significant bit of whatever's being sent
	uint8_t read(uint32_t address);
	void write(uint32_t address, uint8_t value);
private:
	uint64_t ROMData[1024];
	
	int addressSize = 0;

	uint32_t writeCount = 0;
	uint32_t readbackCount = 0;
	uint32_t writeAddress = 0;
	uint32_t readAddress = 0;
	uint64_t readData = 0;
	uint64_t tempWriteBits = 0;
	uint64_t newROMData = 0;
	bool isReading = false;
	bool activeRead = false;
	WriteState state = {};

};