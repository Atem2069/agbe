#include"SRAM.h"

SRAM::SRAM(BackupType type)
{
	std::vector<uint8_t> saveData;
	if (getSaveData(saveData))
	{
		memcpy(mem, (void*)&saveData[0], 32768);
	}
	else
		memset((void*)mem, 0xFF, 32768);
}

SRAM::~SRAM()
{
	writeSaveData((void*)mem, 32768);
}

uint8_t SRAM::read(uint32_t address)
{
	return mem[address & 0x7FFF];
}

void SRAM::write(uint32_t address, uint8_t value)
{
	mem[address & 0x7FFF] = value;
}