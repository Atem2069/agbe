#include"PPU.h"

PPU::PPU()
{
	//simple test
	for (int i = 0; i < (240 * 160); i++)
		m_displayBuffer[i] = i;
}

PPU::~PPU()
{

}

void PPU::registerMemory(std::shared_ptr<GBAMem> mem)
{
	m_mem = mem;
}

void PPU::step()
{

}

uint32_t* PPU::getDisplayBuffer()
{
	return m_displayBuffer;
}

uint8_t PPU::readIO(uint32_t address)
{

}

void PPU::writeIO(uint32_t address, uint8_t value)
{

}