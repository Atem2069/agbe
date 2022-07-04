#include"SRAM.h"

SRAM::SRAM(BackupType type)
{
	//todo: attempt load save
}

SRAM::~SRAM()
{

}

uint8_t SRAM::read(uint32_t address)
{
	return mem[address & 0xFFFF];
}

void SRAM::write(uint32_t address, uint8_t value)
{
	mem[address & 0xFFFF] = value;
}