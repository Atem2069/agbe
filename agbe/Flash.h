#pragma once

#include"Logger.h"
#include"BackupBase.h"
#include<iostream>

enum class FlashState
{
	Ready,
	Command1,
	Command2,
	Operation,
	ChipID,
	PrepareErase,
	PrepareWrite,
	SwitchBank
};

class Flash : public BackupBase
{
public:
	Flash(BackupType type);
	~Flash();

	uint8_t read(uint32_t address);
	void write(uint32_t address, uint8_t value);
private:
	uint8_t m_manufacturerID=0, m_deviceID=0;
	uint8_t flashMem[128 * 1024];
	uint8_t bank = 0;
	FlashState m_state;
};