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
		case 0:
			renderMode0();
			break;
		case 1: case 2: case 5:
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

		uint16_t vcountCmp = ((DISPSTAT >> 8) & 0xFF);
		if ((VCOUNT & 0xFF) == vcountCmp)
		{
			setVCounterFlag(true);
			if ((DISPSTAT >> 5) & 0b1)
				m_interruptManager->requestInterrupt(InterruptType::VCount);
		}

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

void PPU::renderMode0()
{
	bool bg0Enabled = ((DISPCNT >> 8) & 0b1);
	bool bg1Enabled = ((DISPCNT >> 9) & 0b1);
	bool bg2Enabled = ((DISPCNT >> 10) & 0b1);
	bool bg3Enabled = ((DISPCNT >> 11) & 0b1);

	std::array<BGSortItem, 4> bgItems;
	bgItems[3] = { BG0CNT & 0b11,0,bg0Enabled };
	bgItems[2]={BG1CNT & 0b11, 1, bg1Enabled};
	bgItems[1]={BG2CNT & 0b11, 2, bg2Enabled};
	bgItems[0] = { BG3CNT & 0b11, 3, bg3Enabled };
	
	std::sort(bgItems.begin(), bgItems.end(), BGSortItem::sortDescending);

	//render backdrop first (this might be kinda slow)
	uint16_t bd = (m_mem->paletteRAM[0] << 8) | m_mem->paletteRAM[1];
	uint32_t backdropcol = col16to32(bd);
	uint32_t baseAddr = (VCOUNT * 240);
	for (int i = 0; i < 240; i++)
		m_renderBuffer[baseAddr + i] = backdropcol;

	for (int i = 0; i < 4; i++)
	{
		if (bgItems[i].enabled)
			drawBackground(bgItems[i].bgNumber);
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
		uint32_t plotAddr = (VCOUNT * 240) + i;
		m_renderBuffer[plotAddr] = col16to32(col);
	}
}

void PPU::renderMode4()
{
	uint32_t base = 0;
	bool pageFlip = ((DISPCNT >> 4) & 0b1);
	if (pageFlip)
		base = 0xA000;
	for (int i = 0; i < 240; i++)
	{
		uint32_t address = base + (VCOUNT * 240) + i;
		uint8_t curPaletteIdx = m_mem->VRAM[address];
		uint16_t paletteAddress = (uint16_t)curPaletteIdx * 2;
		uint8_t paletteLow = m_mem->paletteRAM[paletteAddress];
		uint8_t paletteHigh = m_mem->paletteRAM[paletteAddress + 1];

		uint16_t paletteData = ((paletteHigh << 8) | paletteLow);
		uint32_t plotAddress = (VCOUNT * 240) + i;
		m_renderBuffer[plotAddress] = col16to32(paletteData);
	}
}

void PPU::drawBackground(int bg)
{
	uint16_t ctrlReg = 0;
	int xScroll = 0, yScroll = 0;
	switch (bg)
	{
	case 0:
		ctrlReg = BG0CNT;
		xScroll = BG0HOFS;
		yScroll = BG0VOFS;
		break;
	case 1:
		ctrlReg = BG1CNT;
		xScroll = BG1HOFS;
		yScroll = BG1VOFS;
		break;
	case 2:
		ctrlReg = BG2CNT;
		xScroll = BG2HOFS;
		yScroll = BG2VOFS;
		break;
	case 3:
		ctrlReg = BG3CNT;
		xScroll = BG3HOFS;
		yScroll = BG3VOFS;
		break;
	}

	uint32_t tileDataBaseBlock = ((ctrlReg >> 2) & 0b11);
	bool hiColor = ((ctrlReg >> 7) & 0b1);
	uint32_t bgMapBaseBlock = ((ctrlReg >> 8) & 0x1F);
	int screenSize = ((ctrlReg >> 14) & 0b11);

	int yCoord = ((VCOUNT + yScroll) % 256);

	uint32_t curLine = ((yCoord / 8) * 32) * 2;
	uint32_t bgMapBaseAddress = (bgMapBaseBlock * 2048) + curLine;

	for (int x = 0; x < 240; x++)
	{

		int xCoord = (x + xScroll) % 256;

		uint32_t curBgAddr = bgMapBaseAddress + ((xCoord/8)*2);
		uint8_t tileLower = m_mem->VRAM[curBgAddr];
		uint8_t tileHigher = m_mem->VRAM[curBgAddr + 1];
		uint16_t tile = (((uint16_t)tileHigher << 8) | tileLower);

		uint32_t tileNumber = tile & 0x3FF;
		uint32_t paletteNumber = ((tile >> 12) & 0xF);
		bool horizontalFlip = ((tile >> 10) & 0b1);
		bool verticalFlip = ((tile >> 11) & 0b1);
		uint32_t paletteMemoryAddr = 0;
		uint32_t tileMapBaseAddress = 0;

		if (!hiColor)	//16 colors, 16 palettes
		{
			tileMapBaseAddress = (tileDataBaseBlock * 16384) + (tileNumber * 32);

			int yMod8 = ((yCoord%8));
			if (verticalFlip)
				yMod8 = 7 - yMod8;

			tileMapBaseAddress += (yMod8 * 4);

			//have correct row of 4 bytes - now need to get correct byte
			//hl hl hl hl
			//(x mod 8) / 2 gives us correct byte
			//then x mod 2 gives us the nibble 

			int xmod8 = (xCoord % 8);
			if (horizontalFlip)
				xmod8 = 7 - xmod8;
			tileMapBaseAddress += (xmod8 / 2);

			uint8_t tileData = m_mem->VRAM[tileMapBaseAddress];
			int colorId = 0;
			if (xmod8 % 2 == 0)
				colorId = tileData & 0xf;
			else
				colorId = ((tileData >> 4) & 0xf);

			if (!colorId)
				continue;

			paletteMemoryAddr = paletteNumber * 32;
			paletteMemoryAddr += (colorId * 2);
		}
		else		//256 colors, 1 palette
		{
			tileMapBaseAddress = (tileDataBaseBlock * 16384) + (tileNumber * 64);
			tileMapBaseAddress += ((VCOUNT % 8) * 8);

			int xmod8 = (x % 8);
			if (horizontalFlip)
				xmod8 = 7 - xmod8;

			tileMapBaseAddress += xmod8;
			uint8_t tileData = m_mem->VRAM[tileMapBaseAddress];
			paletteMemoryAddr = (tileData * 2);
		}

		uint8_t colLow = m_mem->paletteRAM[paletteMemoryAddr];
		uint8_t colHigh = m_mem->paletteRAM[paletteMemoryAddr + 1];
		uint16_t col = ((colHigh << 8) | colLow);
		uint32_t plotAddr = (VCOUNT * 240) + x;
		m_renderBuffer[plotAddr] = col16to32(col);
	}

}

uint32_t PPU::col16to32(uint16_t col)
{
	int red = (col & 0b0000000000011111);
	red = (red << 3) | (red >> 2);
	int green = (col & 0b0000001111100000) >> 5;
	green = (green << 3) | (green >> 2);
	int blue = (col & 0b0111110000000000) >> 10;
	blue = (blue << 3) | (blue >> 2);

	return (red << 24) | (green << 16) | (blue << 8) | 0x000000FF;

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

void PPU::setVCounterFlag(bool value)
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
	case 0x04000008:
		return BG0CNT & 0xFF;
	case 0x04000009:
		return ((BG0CNT >> 8) & 0xFF);
	case 0x0400000A:
		return BG1CNT & 0xFF;
	case 0x0400000B:
		return ((BG1CNT >> 8) & 0xFF);
	case 0x0400000C:
		return BG2CNT & 0xFF;
	case 0x0400000D:
		return ((BG2CNT >> 8) & 0xFF);
	case 0x0400000E:
		return BG3CNT & 0xFF;
	case 0x0400000F:
		return ((BG3CNT >> 8) & 0xFF);
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
	case 0x04000008:
		BG0CNT &= 0xFF00; BG0CNT |= value;
		break;
	case 0x04000009:
		BG0CNT &= 0xFF; BG0CNT |= (value << 8);
		break;
	case 0x0400000A:
		BG1CNT &= 0xFF00; BG1CNT |= value;
		break;
	case 0x0400000B:
		BG1CNT &= 0xFF; BG1CNT |= (value << 8);
		break;
	case 0x0400000C:
		BG2CNT &= 0xFF00; BG2CNT |= value;
		break;
	case 0x0400000D:
		BG2CNT &= 0xFF; BG2CNT |= (value << 8);
		break;
	case 0x0400000E:
		BG3CNT &= 0xFF00; BG3CNT |= value;
		break;
	case 0x0400000F:
		BG3CNT &= 0xFF; BG3CNT |= (value << 8);
		break;
	case 0x04000010:
		BG0HOFS &= 0xFF00; BG0HOFS |= value;
		break;
	case 0x04000011:
		BG0HOFS &= 0xFF; BG0HOFS |= ((value << 8) & 0b1);
		break;
	case 0x04000012:
		BG0VOFS &= 0xFF00; BG0VOFS |= value;
		break;
	case 0x04000013:
		BG0VOFS &= 0xFF; BG0VOFS |= ((value << 8) & 0b1);
		break;
	case 0x04000014:
		BG1HOFS &= 0xFF00; BG1HOFS |= value;
		break;
	case 0x04000015:
		BG1HOFS &= 0xFF; BG1HOFS |= ((value << 8) & 0b1);
		break;
	case 0x04000016:
		BG1VOFS &= 0xFF00; BG1VOFS |= value;
		break;
	case 0x04000017:
		BG1VOFS &= 0xFF; BG1VOFS |= ((value << 8) & 0b1);
		break;
	case 0x04000018:
		BG2HOFS &= 0xFF00; BG2HOFS |= value;
		break;
	case 0x04000019:
		BG2HOFS &= 0xFF; BG2HOFS |= ((value << 8) & 0b1);
		break;
	case 0x0400001A:
		BG2VOFS &= 0xFF00; BG2VOFS |= value;
		break;
	case 0x0400001B:
		BG2VOFS &= 0xFF; BG2VOFS |= ((value << 8) & 0b1);
		break;
	case 0x0400001C:
		BG3HOFS &= 0xFF00; BG3HOFS |= value;
		break;
	case 0x0400001D:
		BG3HOFS &= 0xFF; BG3HOFS |= ((value << 8) & 0b1);
		break;
	case 0x0400001E:
		BG3VOFS &= 0xFF00; BG3VOFS |= value;
		break;
	case 0x0400001F:
		BG3VOFS &= 0xFF; BG3VOFS |= ((value << 8) & 0b1);
		break;
	default:
		//break;
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unknown PPU IO register write {:#x}", address));
	}
}