#include"SRAM.h"

SRAM::SRAM(BackupType type)
{
	std::vector<uint8_t> saveData;
	if (getSaveData(saveData))
	{
		memcpy(mem, (void*)&saveData[0], 65536);
	}
	else
		memset((void*)mem, 0xFF, 65536);
}

SRAM::~SRAM()
{
	writeSaveData((void*)mem, 65536);
}

uint8_t SRAM::read(uint32_t address)
{
	return mem[address & 0xFFFF];
}

void SRAM::write(uint32_t address, uint8_t value)
{
	mem[address & 0xFFFF] = value;
}