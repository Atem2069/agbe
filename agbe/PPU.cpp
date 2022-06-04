#include"PPU.h"

PPU::PPU(std::shared_ptr<InterruptManager> interruptManager)
{
	m_interruptManager = interruptManager;
	VCOUNT = 0;
	inVBlank = false;
	//simple test
	for (int i = 0; i < (240 * 160); i++)
		m_displayBuffer[i] = i;
}

PPU::~PPU()
{

}

void PPU::registerMemory(std::shared_ptr<GBAMem> mem)
{
	registered = true;
	m_mem = mem;
}

void PPU::step()
{
	if (!registered)
	{
		Logger::getInstance()->msg(LoggerSeverity::Error, "No memory source registered! Cannot render");
		return;
	}
	m_lineCycles += 1;
	if (inVBlank)
	{
		VBlank();
		return;
	}

	if (m_lineCycles > 960)	//961-1232=hblank
		HBlank();
	else
		HDraw();            //1-960=hdraw
}

void PPU::HDraw()
{
	if (m_lineCycles == 959)
	{
		uint8_t mode = DISPCNT & 0b111;
		switch (mode)
		{
		case 0: case 1: case 2: case 5:
			//Logger::getInstance()->msg(LoggerSeverity::Error, "Tried to draw with unimplemented video mode");
			//std::cout << (int)mode << '\n';
			break;
		case 3:
			renderMode3();
			break;
		case 4:
			renderMode4();
			break;
		}
	}
}

void PPU::HBlank()
{
	if (m_lineCycles == 961)
	{
		if (((DISPSTAT >> 4) & 0b1))
			m_interruptManager->requestInterrupt(InterruptType::HBlank);
	}
	//todo: check timing of when exactly hblank flag/interrupt set
	setHBlankFlag(true);

	if (m_lineCycles == 1232)
	{
		setHBlankFlag(false);
		m_lineCycles = 0;

		VCOUNT++;

		if (VCOUNT == 160)
		{
			setVBlankFlag(true);
			inVBlank = true;

			if (((DISPSTAT >> 3) & 0b1))
				m_interruptManager->requestInterrupt(InterruptType::VBlank);

			//copy display buf over
			memcpy(m_displayBuffer, m_renderBuffer, 240 * 160 * sizeof(uint32_t));

		}
	}
}

void PPU::VBlank()
{
	setVBlankFlag(true);
	if (m_lineCycles > 960)
		setHBlankFlag(true);
	if (m_lineCycles == 1232)
	{
		setHBlankFlag(false);
		m_lineCycles = 0;

		VCOUNT++;
		if (VCOUNT == 228)		//go back to drawing
		{
			setVBlankFlag(false);
			inVBlank = false;
			VCOUNT = 0;
		}
	}
}

void PPU::renderMode3()
{
	for (int i = 0; i < 240; i++)
	{
		uint32_t address = (VCOUNT * 480) + (i*2);
		uint8_t colLow = m_mem->VRAM[address];
		uint8_t colHigh = m_mem->VRAM[address + 1];
		uint16_t col = ((colHigh << 8) | colLow);

		int red = (col & 0b0000000000011111);
		red = (red << 3) | (red >> 2);
		int green = (col & 0b0000001111100000) >> 5;
		green = (green << 3) | (green >> 2);
		int blue = (col & 0b0111110000000000) >> 10;
		blue = (blue << 3) | (blue >> 2);

		uint32_t res = (red << 24) | (green << 16) | (blue << 8) | 0x000000FF;
		uint32_t plotAddr = (VCOUNT * 240) + i;
		m_renderBuffer[plotAddr] = res;
	}
}

void PPU::renderMode4()
{
	for (int i = 0; i < 240; i++)
	{
		uint32_t address = (VCOUNT * 240) + i;
		uint8_t curPaletteIdx = m_mem->VRAM[address];
		uint16_t paletteAddress = (uint16_t)curPaletteIdx * 2;
		uint8_t paletteLow = m_mem->paletteRAM[paletteAddress];
		uint8_t paletteHigh = m_mem->paletteRAM[paletteAddress + 1];

		uint16_t paletteData = ((paletteHigh << 8) | paletteLow);

		int red = (paletteData & 0b0000000000011111);
		red = (red << 3) | (red >> 2);
		int green = (paletteData & 0b0000001111100000) >> 5;
		green = (green << 3) | (green >> 2);
		int blue = (paletteData & 0b0111110000000000) >> 10;
		blue = (blue << 3) | (blue >> 2);

		uint32_t res = (red << 24) | (green << 16) | (blue << 8) | 0x000000FF;
		m_renderBuffer[address] = res;
	}
}


uint32_t* PPU::getDisplayBuffer()
{
	return m_displayBuffer;
}

void PPU::setVBlankFlag(bool value)
{
	if (value)
		DISPSTAT |= 0b1;
	else
		DISPSTAT &= ~0b1;
}

void PPU::setHBlankFlag(bool value)
{
	if (value)
		DISPSTAT |= 0b10;
	else
		DISPSTAT &= ~0b10;
}

bool PPU::setVCounterFlag(bool value)
{
	if (value)
		DISPSTAT |= 0b100;
	else
		DISPSTAT &= ~0b100;
}

uint8_t PPU::readIO(uint32_t address)
{
	switch (address)
	{
	case 0x04000000:
		return DISPCNT & 0xFF;
	case 0x04000001:
		return ((DISPCNT >> 8) & 0xFF);
	case 0x04000004:
		return DISPSTAT & 0xFF;
	case 0x04000005:
		return ((DISPSTAT >> 8) & 0xFF);
	case 0x04000006:
		return VCOUNT & 0xFF;
	case 0x04000007:
		return ((VCOUNT >> 8) & 0xFF);
	}

	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unknown PPU IO register read {:#x}", address));
	return 0;
}

void PPU::writeIO(uint32_t address, uint8_t value)
{
	switch (address)
	{
	case 0x04000000:
		DISPCNT &= 0xFF00; DISPCNT |= value;
		break;
	case 0x04000001:
		DISPCNT &= 0x00FF; DISPCNT |= (value << 8);
		break;
	case 0x04000004:
		DISPSTAT &= 0b1111111100000111; value &= 0b11111000;
		DISPSTAT |= value;
		break;
	case 0x04000005:
		DISPSTAT &= 0x00FF; DISPSTAT |= (value << 8);
		break;
	default:
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unknown PPU IO register write {:#x}", address));
	}
}