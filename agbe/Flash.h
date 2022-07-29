#pragma once

#include"Logger.h"
#include"BackupBase.h"
#include<iostream>

enum class FlashState
{
	Ready,						//default state
	CommandInProgress,			//set after first cmd byte written to E005555
	Operation,					//set after second cmd byte written to E002AAA
	PrepareWrite,				//set after 'A0' write to E005555 in 'Operation' mode - single byte about to be written
	BankSwitch					//set after 'B0' write to E005555 in 'Operation' mode - 1 bit bank number about to be written to E000000
};

class Flash : public BackupBase
{
public:
	Flash(BackupType type);
	~Flash();

	uint8_t read(uint32_t address);
	void write(uint32_t address, uint8_t value);
private:
	bool inChipID = false;
	uint8_t m_manufacturerID=0, m_deviceID=0;
	uint8_t flashMem[128 * 1024];
	uint8_t bank = 0;
	FlashState m_state;
};