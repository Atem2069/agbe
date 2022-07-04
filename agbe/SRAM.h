#pragma once

#include"Logger.h"
#include"BackupBase.h"

class SRAM : public BackupBase
{
public:
	SRAM(BackupType type);
	~SRAM();

	uint8_t read(uint32_t address);
	void write(uint32_t address, uint8_t value);
private:
	uint8_t mem[65536];
};