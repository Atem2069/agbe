#pragma once

#include"Logger.h"
#include"GBAMem.h"

class PPU
{
public:
	PPU();
	~PPU();

	void registerMemory(std::shared_ptr<GBAMem> mem);

	void step();

	uint32_t* getDisplayBuffer();

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);

private:
	std::shared_ptr<GBAMem> m_mem;
	uint32_t m_renderBuffer[240 * 160];	//currently being rendered
	uint32_t m_displayBuffer[240 * 160]; //buffer the display gets
};