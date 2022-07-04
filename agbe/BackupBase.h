#pragma once

#include"Logger.h"

enum class BackupType
{
	None,
	SRAM,
	EEPROM4K,
	EEPROM64K,
	FLASH512K,
	FLASH1M
};

class BackupBase
{
public:
	BackupBase() {};
	BackupBase(BackupType type) {};
	~BackupBase() {};

	virtual uint8_t read(uint32_t address) { return 0; };
	virtual void write(uint32_t address, uint8_t value) {};
};