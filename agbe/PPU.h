#pragma once

#include"Logger.h"
#include"GBAMem.h"
#include"InterruptManager.h"

class PPU
{
public:
	PPU(std::shared_ptr<InterruptManager> interruptManager);
	~PPU();

	void registerMemory(std::shared_ptr<GBAMem> mem);

	void step();

	uint32_t* getDisplayBuffer();

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);

private:
	bool registered = false;
	std::shared_ptr<GBAMem> m_mem;
	std::shared_ptr<InterruptManager> m_interruptManager;
	uint32_t m_renderBuffer[240 * 160];	//currently being rendered
	uint32_t m_displayBuffer[240 * 160]; //buffer the display gets

	uint32_t m_lineCycles = 0;
	bool inVBlank = false;

	void HDraw();
	void HBlank();
	void VBlank();

	void renderMode0();
	void renderMode3();
	void renderMode4();

	void drawBackground(int bg);

	void setVBlankFlag(bool value);
	void setHBlankFlag(bool value);
	void setVCounterFlag(bool value);

	uint32_t col16to32(uint16_t col);

	uint16_t DISPCNT = {};
	uint16_t DISPSTAT = {};
	uint16_t VCOUNT = {};
	uint16_t BG0CNT = {};
	uint16_t BG1CNT = {};
	uint16_t BG2CNT = {};
	uint16_t BG3CNT = {};
	uint16_t BG0HOFS = {};
	uint16_t BG0VOFS = {};
	uint16_t BG1VOFS = {};
	uint16_t BG2VOFS = {};
	uint16_t BG3VOFS = {};
};