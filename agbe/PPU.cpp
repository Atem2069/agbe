#include"PPU.h"

PPU::PPU(std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<Scheduler> scheduler)
{
	m_scheduler = scheduler;
	m_interruptManager = interruptManager;
	VCOUNT = 0;
	inVBlank = false;
	//simple test
	for (int i = 0; i < (240 * 160); i++)
		m_renderBuffer[pageIdx][i] = i;
	for (int i = 0; i < 240; i++)
		m_bgPriorities[i] = 255;

	m_state = PPUState::HDraw;
	m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, 960);	//960 cycles from now, do hdraw for vcount=0
}

PPU::~PPU()
{

}

void PPU::registerMemory(std::shared_ptr<GBAMem> mem)
{
	registered = true;
	m_mem = mem;
}

void PPU::eventHandler()
{
	uint64_t schedTimestamp = m_scheduler->getCurrentTimestamp();
	uint64_t timeDiff = schedTimestamp - expectedNextTimeStamp;

	switch (m_state)
	{
	case PPUState::HDraw:
		HDraw();
		m_state = PPUState::HBlank;
		expectedNextTimeStamp = (schedTimestamp + 46) - timeDiff;
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, expectedNextTimeStamp);
		break;
	case PPUState::HBlank:
		HBlank();
		break;
	case PPUState::VBlank:
		VBlank();
		break;
	}
}

void PPU::HDraw()
{
	uint8_t mode = DISPCNT & 0b111;
	switch (mode)
	{
	case 0:
		renderMode0();
		break;
	case 1:
		renderMode1();
		break;
	case 2:
		renderMode2();
		break;
	case 3:
		renderMode3();
		break;
	case 4:
		renderMode4();
		break;
	case 5:
		break;
	}

	//todo: check timing of hblank flag
	if (((DISPSTAT >> 4) & 0b1))
		m_interruptManager->requestInterrupt(InterruptType::HBlank);
	DMAHBlankCallback(callbackContext);
}

void PPU::HBlank()
{
	uint64_t schedTimestamp = m_scheduler->getCurrentTimestamp();
	uint64_t timeDiff = schedTimestamp - expectedNextTimeStamp;
	//todo: check timing of when exactly hblank flag/interrupt set

	if (!hblank_flagSet)	//now set hblank flag, at cycle 1006!
	{
		hblank_flagSet = true;
		setHBlankFlag(true);
		expectedNextTimeStamp = (schedTimestamp + 226) - timeDiff;
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, expectedNextTimeStamp);
		return;
	}

	hblank_flagSet = false;

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
	else
		setVCounterFlag(false);

	if (VCOUNT == 160)
	{
		//copy over new values to affine regs
		BG2X = BG2X_latch;
		BG2Y = BG2Y_latch;
		BG3X = BG3X_latch;
		BG3Y = BG3Y_latch;

		setVBlankFlag(true);
		inVBlank = true;

		if (((DISPSTAT >> 3) & 0b1))
			m_interruptManager->requestInterrupt(InterruptType::VBlank);

		pageIdx = !pageIdx;

		m_state = PPUState::VBlank;
		expectedNextTimeStamp = (schedTimestamp + 1006) - timeDiff;
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, expectedNextTimeStamp);

		DMAVBlankCallback(callbackContext);

		return;
	}
	else
		m_state = PPUState::HDraw;

	expectedNextTimeStamp = (schedTimestamp + 960) - timeDiff;
	m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, expectedNextTimeStamp);
}

void PPU::VBlank()
{
	uint64_t schedTimestamp = m_scheduler->getCurrentTimestamp();
	uint64_t timeDiff = schedTimestamp - expectedNextTimeStamp;
	setVBlankFlag(true);
	//if (m_lineCycles > 960)
	//	setHBlankFlag(true);
	m_lineCycles = 0;
	m_state=PPUState::VBlank;

	if (!vblank_setHblankBit)	//handle first part of line, up until hblank
	{
		setHBlankFlag(true);
		vblank_setHblankBit = true;
		expectedNextTimeStamp = (schedTimestamp + 226) - timeDiff;
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, expectedNextTimeStamp);
		return;
	}

	vblank_setHblankBit = false;	//end of vblank for current line
	setHBlankFlag(false);
	VCOUNT++;
	if (VCOUNT == 228)		//go back to drawing
	{

		uint16_t vcountCmp = ((DISPSTAT >> 8) & 0xFF);
		if (vcountCmp==0)
		{
			setVCounterFlag(true);
			if ((DISPSTAT >> 5) & 0b1)
				m_interruptManager->requestInterrupt(InterruptType::VCount);
		}
		else
			setVCounterFlag(false);

		setVBlankFlag(false);
		inVBlank = false;
		shouldSyncVideo = true;
		VCOUNT = 0;
		m_state = PPUState::HDraw;
		expectedNextTimeStamp = (schedTimestamp + 960) - timeDiff;
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, expectedNextTimeStamp);
	}
	else
	{
		if (VCOUNT == 227)			//set vblank flag false on last line of vblank (if applicable)
			setVBlankFlag(false);
		uint16_t vcountCmp = ((DISPSTAT >> 8) & 0xFF);
		if ((VCOUNT & 0xFF) == vcountCmp)
		{
			setVCounterFlag(true);
			if ((DISPSTAT >> 5) & 0b1)
				m_interruptManager->requestInterrupt(InterruptType::VCount);
		}
		else
			setVCounterFlag(false);
		expectedNextTimeStamp = (schedTimestamp + 1006) - timeDiff;
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, expectedNextTimeStamp);
	}

}

void PPU::renderMode0()
{
	bool objEnabled = ((DISPCNT >> 12) & 0b1);
	if (objEnabled)
		drawSprites();

	bool bg0Enabled = ((DISPCNT >> 8) & 0b1);
	bool bg1Enabled = ((DISPCNT >> 9) & 0b1);
	bool bg2Enabled = ((DISPCNT >> 10) & 0b1);
	bool bg3Enabled = ((DISPCNT >> 11) & 0b1);

	std::array<BGSortItem, 4> bgItems;
	bgItems[3] = { BG0CNT & 0b11,0,bg0Enabled,false };
	bgItems[2]={BG1CNT & 0b11, 1, bg1Enabled,false};
	bgItems[1]={BG2CNT & 0b11, 2, bg2Enabled,false};
	bgItems[0] = { BG3CNT & 0b11, 3, bg3Enabled,false };
	
	std::sort(bgItems.begin(), bgItems.end(), BGSortItem::sortDescending);

	uint16_t bd = (m_mem->paletteRAM[1] << 8) | m_mem->paletteRAM[0];
	uint32_t backdropcol = col16to32(bd);
	uint32_t baseAddr = (VCOUNT * 240);

	for (int i = 0; i < 4; i++)	//todo: optimise. can use a single for loop and get each pixel one by one
	{
		if (bgItems[i].enabled)
			drawBackground(bgItems[i].bgNumber);
	}
	for (int i = 0; i < 240; i++)
	{
		if (m_bgPriorities[i] == 255)
		{
			m_renderBuffer[pageIdx][baseAddr + i] = backdropcol;
			if (m_spritePriorities[i] != 255)
				m_renderBuffer[pageIdx][baseAddr + i] = m_spriteLineBuffer[i];
		}
		m_bgPriorities[i] = 255;
		target2Pixels[i] = 0;
	}
}

void PPU::renderMode1()
{
	bool objEnabled = ((DISPCNT >> 12) & 0b1);
	if (objEnabled)
		drawSprites();

	bool bg0Enabled = ((DISPCNT >> 8) & 0b1);
	bool bg1Enabled = ((DISPCNT >> 9) & 0b1);
	bool bg2Enabled = ((DISPCNT >> 10) & 0b1);

	std::array<BGSortItem, 3> bgItems;
	bgItems[2] = { BG0CNT & 0b11,0,bg0Enabled,false };
	bgItems[1] = { BG1CNT & 0b11, 1, bg1Enabled,false };
	bgItems[0] = { BG2CNT & 0b11, 2, bg2Enabled,true };

	std::sort(bgItems.begin(), bgItems.end(), BGSortItem::sortDescending);

	uint16_t bd = (m_mem->paletteRAM[1] << 8) | m_mem->paletteRAM[0];
	uint32_t backdropcol = col16to32(bd);
	uint32_t baseAddr = (VCOUNT * 240);

	for (int i = 0; i < 3; i++)	//todo: optimise. can use a single for loop and get each pixel one by one
	{
		if (bgItems[i].enabled)
		{
			if (!bgItems[i].affine)
				drawBackground(bgItems[i].bgNumber);
			else
				drawRotationScalingBackground(bgItems[i].bgNumber);
		}
	}
	for (int i = 0; i < 240; i++)
	{
		if (m_bgPriorities[i] == 255)
		{
			m_renderBuffer[pageIdx][baseAddr + i] = backdropcol;
			if (m_spritePriorities[i] != 255)
				m_renderBuffer[pageIdx][baseAddr + i] = m_spriteLineBuffer[i];
		}
		m_bgPriorities[i] = 255;
		target2Pixels[i] = 0;
	}
}

void PPU::renderMode2()
{
	drawSprites();
	bool bg2Enabled = ((DISPCNT >> 10) & 0b1);
	bool bg3Enabled = ((DISPCNT >> 11) & 0b1);
	
	uint16_t bd = (m_mem->paletteRAM[1] << 8) | m_mem->paletteRAM[0];
	uint32_t backdropcol = col16to32(bd);
	uint32_t baseAddr = (VCOUNT * 240);
	//todo: priority
	if(bg3Enabled)
		drawRotationScalingBackground(3);
	if(bg2Enabled)
		drawRotationScalingBackground(2);
	for (int i = 0; i < 240; i++)
	{
		if (m_bgPriorities[i] == 255)
		{
			m_renderBuffer[pageIdx][baseAddr + i] = backdropcol;
			if (m_spritePriorities[i] != 255)
				m_renderBuffer[pageIdx][baseAddr + i] = m_spriteLineBuffer[i];
		}
		m_bgPriorities[i] = 255;
		target2Pixels[i] = 0;
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
		m_renderBuffer[pageIdx][plotAddr] = col16to32(col);
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
		m_renderBuffer[pageIdx][plotAddress] = col16to32(paletteData);
	}
}

void PPU::drawBackground(int bg)
{
	uint16_t ctrlReg = 0;
	uint32_t xScroll = 0, yScroll = 0;
	bool isTarget1 = ((BLDCNT >> bg) & 0b1);
	bool isTarget2 = ((BLDCNT >> (bg + 8)) & 0b1);
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

	//xScroll &= 0x1FF;
	//yScroll &= 0x1FF;

	uint8_t bgPriority = ctrlReg & 0b11;
	uint32_t tileDataBaseBlock = ((ctrlReg >> 2) & 0b11);
	bool hiColor = ((ctrlReg >> 7) & 0b1);
	uint32_t bgMapBaseBlock = ((ctrlReg >> 8) & 0x1F);
	int screenSize = ((ctrlReg >> 14) & 0b11);
	int xWrap = 255, yWrap = 255;
	switch (screenSize)
	{
	case 1:
		xWrap = 511;
		break;
	case 2:
		yWrap = 511;
		break;
	case 3:
		xWrap = 511;
		yWrap = 511;
		break;
	}

	uint32_t fetcherY = ((VCOUNT + yScroll) & yWrap);
	if (screenSize && (fetcherY > 255))
	{
		fetcherY -= 256;
		bgMapBaseBlock += 1;	
		if (screenSize == 3)	//not completely sure
			bgMapBaseBlock += 1;
	}
	uint32_t bgMapYIdx = ((fetcherY / 8) * 32) * 2; //each row is 32 chars - each char is 2 bytes
	for (int x = 0; x < 240; x++)
	{
		uint32_t plotAddr = (VCOUNT * 240) + x;
		if (m_spritePriorities[x] <= bgPriority)
		{
			m_renderBuffer[pageIdx][plotAddr] = m_spriteLineBuffer[x];
			m_bgPriorities[x] = 254;	//lmfao
			continue;
		}
		if (!getPointDrawable(x, VCOUNT, bg, false))
			continue;
		int xCoord = (x + xScroll) & xWrap;
		int baseBlockOffset = 0;				//x scrolling can cause new baseblock to bee selected
		if (screenSize && (xCoord > 255))
		{
			xCoord -= 256;
			baseBlockOffset += 1;
		}
		uint32_t bgMapBaseAddress = ((bgMapBaseBlock+baseBlockOffset) * 2048) + bgMapYIdx;
		uint32_t curBgAddr = bgMapBaseAddress + (((xCoord/8)*2));
		uint8_t tileLower = m_mem->VRAM[curBgAddr];
		uint8_t tileHigher = m_mem->VRAM[curBgAddr + 1];
		uint16_t tile = (((uint16_t)tileHigher << 8) | tileLower);

		uint32_t tileNumber = tile & 0x3FF;
		uint32_t paletteNumber = ((tile >> 12) & 0xF);
		bool horizontalFlip = ((tile >> 10) & 0b1);
		bool verticalFlip = ((tile >> 11) & 0b1);
		uint32_t paletteMemoryAddr = 0;
		uint32_t tileMapBaseAddress = 0;

		int yMod8 = ((fetcherY & 7));
		if (verticalFlip)
			yMod8 = 7 - yMod8;

		if (!hiColor)	//16 colors, 16 palettes
		{
			tileMapBaseAddress = (tileDataBaseBlock * 16384) + (tileNumber * 32);
			tileMapBaseAddress += (yMod8 * 4);

			//have correct row of 4 bytes - now need to get correct byte
			//hl hl hl hl
			//(x mod 8) / 2 gives us correct byte
			//then x mod 2 gives us the nibble 

			int xmod8 = (xCoord & 7);
			if (horizontalFlip)
				xmod8 = 7 - xmod8;
			tileMapBaseAddress += (xmod8 / 2);

			uint8_t tileData = m_mem->VRAM[tileMapBaseAddress];
			int colorId = 0;
			int stepTile = ((xmod8 & 0b1));
			colorId = ((tileData >> (stepTile * 4)) & 0xf);	//first (even) pixel - low nibble. second (odd) pixel - high nibble
			if (!colorId)
				continue;

			paletteMemoryAddr = paletteNumber * 32;
			paletteMemoryAddr += (colorId * 2);
		}
		else		//256 colors, 1 palette
		{
			//this is completely wrong !! todo: fix
			tileMapBaseAddress = (tileDataBaseBlock * 16384) + (tileNumber * 64);
			tileMapBaseAddress += ((yMod8) * 8);

			int xmod8 = (xCoord & 7);
			if (horizontalFlip)
				xmod8 = 7 - xmod8;

			tileMapBaseAddress += xmod8;
			uint8_t tileData = m_mem->VRAM[tileMapBaseAddress];
			if (!tileData)
				continue;
			paletteMemoryAddr = (tileData * 2);
		}

		uint8_t colLow = m_mem->paletteRAM[paletteMemoryAddr];
		uint8_t colHigh = m_mem->paletteRAM[paletteMemoryAddr + 1];
		uint16_t col = ((colHigh << 8) | colLow);
		m_bgPriorities[x] = bgPriority;

		if (getPointBlendable(x, VCOUNT))
		{
			if (isTarget2)
				target2Pixels[x] = col;
			if (isTarget1)
			{
				uint8_t blendMode = ((BLDCNT >> 6) & 0b11);
				switch (blendMode)
				{
				case 1:
					col = blendAlpha(col, target2Pixels[x]);
					break;
				case 2:
					col = blendBrightness(col, true);
					break;
				case 3:
					col = blendBrightness(col, false);
					break;
				}
			}
		}

		m_renderBuffer[pageIdx][plotAddr] = col16to32(col);
	}

}

void PPU::drawRotationScalingBackground(int bg)
{
	uint16_t ctrlReg = 0;
	int32_t xRef = 0, yRef = 0;
	int16_t pA = 0, pB = 0, pC = 0, pD = 0;
	bool isTarget1 = ((BLDCNT >> bg) & 0b1);
	bool isTarget2 = ((BLDCNT >> (bg + 8)) & 0b1);
	switch (bg)
	{
	case 2:
		ctrlReg = BG2CNT;
		xRef = BG2X&0xFFFFFFF;
		yRef = BG2Y&0xFFFFFFF;
		pA = BG2PA; pB = BG2PB; pC = BG2PC; pD = BG2PD;
		if ((BG2X >> 27) & 0b1)
			xRef |= 0xF0000000;
		if ((BG2Y >> 27) & 0b1)
			yRef |= 0xF0000000;
		break;
	case 3:
		ctrlReg = BG3CNT;
		xRef = BG3X&0xFFFFFFF;
		yRef = BG3Y&0xFFFFFFF;
		pA = BG3PA; pB = BG3PB; pC = BG3PC; pD = BG3PD;
		if ((BG3X >> 27) & 0b1)
			xRef |= 0xF0000000;
		if ((BG3Y >> 27) & 0b1)
			yRef |= 0xF0000000;
		break;
	}
	int xWrap = 256, yWrap = 256;
	int screenSize = ((ctrlReg >> 14) & 0b11);	//could optimise this with a lut fwiw
	switch (screenSize)
	{
	case 0:
		xWrap = 128;
		yWrap = 128;
		break;
	case 2:
		xWrap = 512;
		yWrap = 512;
		break;
	case 3:
		xWrap = 1024;
		yWrap = 1024;
		break;
	}
	uint8_t bgPriority = ctrlReg & 0b11;
	uint32_t tileDataBaseBlock = ((ctrlReg >> 2) & 0b11);
	uint32_t bgMapBaseBlock = ((ctrlReg >> 8) & 0x1F);
	bool overflowWrap = ((ctrlReg >> 13) & 0b1);

	for (int x = 0; x < 240; x++, xRef+=pA,yRef+=pC)
	{
		uint32_t plotAddr = (VCOUNT * 240) + x;
		if (m_spritePriorities[x] <= bgPriority)
		{
			m_renderBuffer[pageIdx][plotAddr] = m_spriteLineBuffer[x];
			m_bgPriorities[x] = 254;	//dumb hack :P
			continue;
		}
		if (!getPointDrawable(x, VCOUNT, bg, false))
			continue;

		uint32_t fetcherY = (uint32_t)((int32_t)yRef >> 8);
		if ((fetcherY >= yWrap) && !overflowWrap)
			continue;
		fetcherY = fetcherY % yWrap;
		uint32_t bgMapYIdx = ((fetcherY / 8) * (xWrap>>3)); //each row is 32 chars; in rotation/scroll each entry is 1 byte

		uint32_t xCoord = (uint32_t)((int32_t)xRef >> 8);
		if ((xCoord >= xWrap) && !overflowWrap)
			continue;
		xCoord = xCoord % xWrap;

		uint32_t bgMapAddr = (bgMapBaseBlock * 2048) + bgMapYIdx;
		bgMapAddr += (xCoord/8);

		uint32_t tileIdx = m_mem->VRAM[bgMapAddr];

		uint32_t tileMapBaseAddress = (tileDataBaseBlock * 16384) + (tileIdx * 64);
		tileMapBaseAddress += ((fetcherY % 8) * 8);

		int xmod8 = (xCoord % 8);
		tileMapBaseAddress += xmod8;
		uint8_t tileData = m_mem->VRAM[tileMapBaseAddress];
		if (!tileData)
			continue;
		uint32_t paletteMemoryAddr = (tileData * 2);

		uint8_t colLow = m_mem->paletteRAM[paletteMemoryAddr];
		uint8_t colHigh = m_mem->paletteRAM[paletteMemoryAddr + 1];
		uint16_t col = ((colHigh << 8) | colLow);
		m_bgPriorities[x] = bgPriority;

		if (getPointBlendable(x, VCOUNT))
		{
			if (isTarget2)
				target2Pixels[x] = col;
			if (isTarget1)
			{
				uint8_t blendMode = ((BLDCNT >> 6) & 0b11);
				switch (blendMode)
				{
				case 1:
					col = blendAlpha(col, target2Pixels[x]);
					break;
				case 2:
					col = blendBrightness(col, true);
					break;
				case 3:
					col = blendBrightness(col, false);
					break;
				}
			}
		}

		m_renderBuffer[pageIdx][plotAddr] = col16to32(col);

	}

	updateAffineRegisters(bg);
}

void PPU::drawSprites()
{
	bool oneDimensionalMapping = ((DISPCNT >> 6) & 0b1);
	bool isBlendTarget1 = ((BLDCNT >> 4) & 0b1);
	bool isBlendTarget2 = ((BLDCNT >> 12) & 0b1);
	uint8_t blendMode = ((BLDCNT >> 6) & 0b11);

	for (int i = 0; i < 240; i++)
	{
		m_objWindowMask[i] = 0;
		m_spritePriorities[i] = 255;
	}

	for (int i = 127; i >= 0; i--)
	{
		uint32_t spriteBase = i * 8;	//each OAM entry is 8 bytes
		uint8_t attr0Low = m_mem->OAM[spriteBase];
		uint8_t attr0High = m_mem->OAM[spriteBase + 1];
		uint8_t attr1Low = m_mem->OAM[spriteBase + 2];
		uint8_t attr1High = m_mem->OAM[spriteBase + 3];
		uint8_t attr2Low = m_mem->OAM[spriteBase + 4];
		uint8_t attr2High = m_mem->OAM[spriteBase + 5];
		uint16_t attr0 = ((attr0High << 8) | attr0Low);
		uint16_t attr1 = ((attr1High << 8) | attr1Low);
		uint16_t attr2 = ((attr2High << 8) | attr2Low);

		if ((attr0 >> 8) & 0b1)
		{
			drawAffineSprite(i);
			continue;
		}

		bool spriteDisabled = ((attr0 >> 9) & 0b1);	
		if (spriteDisabled)
			continue;

		uint8_t objMode = (attr0 >> 10) & 0b11;	//have to accommodate for the other obj modes.
		bool isObjWindow = (objMode == 2);

		int spriteTop = attr0 & 0xFF;
		if (spriteTop > 225)							//bit of a dumb hack to accommodate for when sprites are offscreen
			spriteTop = 0 - (255 - spriteTop);
		int spriteLeft = attr1 & 0x1FF;
		if ((spriteLeft >> 8) & 0b1)
			spriteLeft |= 0xFFFFFF00;	//not sure maybe sign extension is okay
		if (spriteLeft >= 240 || spriteTop > VCOUNT)	//nope. sprite is offscreen or too low
			continue;
		int spriteBottom = 0, spriteRight = 0;
		int rowPitch = 1;	//find out how many lines we have to 'cross' to get to next row (in 1d mapping)
		//need to find out dimensions first to figure out whether to ignore this object
		int shape = ((attr0 >> 14) & 0b11);
		int size = ((attr1 >> 14) & 0b11);
		int spritePriority = ((attr2 >> 10) & 0b11);	//used to figure out whether we should actually render the sprite

		int spriteBoundsLookupId = (shape << 2) | size;
		int spriteXBoundsLUT[12] ={8,16,32,64,16,32,32,64,8,8,16,32};
		int spriteYBoundsLUT[12] ={8,16,32,64,8,8,16,32,16,32,32,64};
		int xPitchLUT[12] ={1,2,4,8,2,4,4,8,1,1,2,4};

		spriteRight = spriteLeft + spriteXBoundsLUT[spriteBoundsLookupId];
		spriteBottom = spriteTop + spriteYBoundsLUT[spriteBoundsLookupId];
		rowPitch = xPitchLUT[spriteBoundsLookupId];	
		if (VCOUNT >= spriteBottom)	//nope, we're past it.
			continue;

		bool flipHorizontal = ((attr1 >> 12) & 0b1);
		bool flipVertical = ((attr1 >> 13) & 0b1);

		int spriteYSize = (spriteBottom - spriteTop);	//find out how big sprite is
		int yOffsetIntoSprite = VCOUNT - spriteTop;
		if (flipVertical)
			yOffsetIntoSprite = (spriteYSize-1) - yOffsetIntoSprite;//flip y coord we're considering

		uint32_t tileId = ((attr2) & 0x3FF);
		uint8_t priorityBits = ((attr2 >> 10) & 0b11);
		uint8_t paletteNumber = ((attr2 >> 12) & 0xF);
		bool hiColor = ((attr0 >> 13) & 0b1);
		if (hiColor)
			rowPitch *= 2;
		//if (hiColor)
		//	std::cout << "sprite can't render! unsupported mode!!" << '\n';


		//check y coord (ASSUMING 2D MAPPING). tiles are arranged in 32x32 (each tile is 32 bytes). so need to add offset if y too big
		while (yOffsetIntoSprite >= 8)
		{
			yOffsetIntoSprite -= 8;
			if (!oneDimensionalMapping)
				tileId += 32;	//add 32 to get to next tile row with 2d mapping
			else
				tileId += rowPitch; //otherwise, add the row pitch (which says how many tiles exist per row)
		}

		//for testing: only draw the first tile
		uint32_t objBase = 0x10000;
		if (!hiColor)
		{
			objBase += (tileId * 32);
			objBase += (yOffsetIntoSprite * 4);	//finally add corrected y offset
		}
		else
		{
			objBase += ((tileId) * 32);
			objBase += (yOffsetIntoSprite * 8);
		}

		int numXTilesToRender = (spriteRight - spriteLeft) / 8;
		for (int xSpanTile = 0; xSpanTile < numXTilesToRender; xSpanTile++)
		{
			int curXSpanTile = xSpanTile;
			if (flipHorizontal)
				curXSpanTile = ((numXTilesToRender - 1) - curXSpanTile);	//flip render order if horizontal flip !!
			uint32_t tileMapLookupAddr = objBase + (curXSpanTile * ((hiColor) ? 64 : 32));

			for (int x = 0; x < 8; x++)
			{
				int baseX = x;
				if (flipHorizontal)
					baseX = 7 - baseX;

				int plotCoord = (xSpanTile * 8) + x + spriteLeft;
				if (plotCoord > 239 || plotCoord < 0)
					continue;

				if (!getPointDrawable(plotCoord, VCOUNT, 0, true) && !isObjWindow)	//not sure about the 'isObjWindow' check
					continue;

				//let's see if a bg pixel with higher priority already exists! (todo: check for sprite priority too)
				//int priorityAtPixel = m_bgPriorities[plotCoord];
				//if (spritePriority > priorityAtPixel)
				//	continue;
				uint8_t priorityAtPixel = m_spritePriorities[plotCoord];
				if ((spritePriority > priorityAtPixel) && !isObjWindow)
					continue;

				int paletteAddr = 0x200;
				if (!hiColor)
				{
					uint8_t tileData = m_mem->VRAM[tileMapLookupAddr + (baseX / 2)];
					int paletteId = tileData & 0xF;
					if (baseX & 0b1)
						paletteId = ((tileData >> 4) & 0xF);

					if (!paletteId)		//don't render if palette id == 0
						continue;

					paletteAddr += ((int)(paletteNumber) * 32) + (paletteId * 2);
				}
				else
				{
					uint8_t tileData = m_mem->VRAM[tileMapLookupAddr + baseX];
					int paletteId = tileData;
					if (!paletteId)
						continue;
					paletteAddr += (paletteId * 2);
				}
				uint8_t colLow = m_mem->paletteRAM[paletteAddr];
				uint8_t colHigh = m_mem->paletteRAM[paletteAddr + 1];
				uint16_t col = ((colHigh << 8) | colLow);

				if (isObjWindow)
					m_objWindowMask[plotCoord] = 1;
				else
				{
					m_spritePriorities[plotCoord] = spritePriority;

					if (getPointBlendable(plotCoord, VCOUNT))
					{
						if (isBlendTarget2)
							target2Pixels[plotCoord] = col;
						if (isBlendTarget1)
						{
							switch (blendMode)
							{
							case 2:
								col = blendBrightness(col, true);
								break;
							case 3:
								col = blendBrightness(col, false);
								break;
							}
						}
					}

					uint32_t res = col16to32(col);
					m_spriteLineBuffer[plotCoord] = res;
				}
				//m_renderBuffer[plotCoord] = res;
			}
		}

	}
}

void PPU::drawAffineSprite(int spriteIdx)
{
	bool oneDimensionalMapping = ((DISPCNT >> 6) & 0b1);
	bool isBlendTarget1 = ((BLDCNT >> 4) & 0b1);
	bool isBlendTarget2 = ((BLDCNT >> 12) & 0b1);
	uint8_t blendMode = ((BLDCNT >> 6) & 0b11);

	uint32_t spriteBase = spriteIdx * 8;	//each OAM entry is 8 bytes
	uint8_t attr0Low = m_mem->OAM[spriteBase];
	uint8_t attr0High = m_mem->OAM[spriteBase + 1];
	uint8_t attr1Low = m_mem->OAM[spriteBase + 2];
	uint8_t attr1High = m_mem->OAM[spriteBase + 3];
	uint8_t attr2Low = m_mem->OAM[spriteBase + 4];
	uint8_t attr2High = m_mem->OAM[spriteBase + 5];
	uint16_t attr0 = ((attr0High << 8) | attr0Low);
	uint16_t attr1 = ((attr1High << 8) | attr1Low);
	uint16_t attr2 = ((attr2High << 8) | attr2Low);

	uint8_t objMode = (attr0 >> 10) & 0b11;
	bool isObjWindow = (objMode == 2);
	bool doubleSize = (attr0 >> 9) & 0b1;

	int spriteTop = attr0 & 0xFF;
	if (spriteTop > 225)							//bit of a dumb hack to accommodate for when sprites are offscreen
		spriteTop = 0 - (255 - spriteTop);
	int spriteLeft = attr1 & 0x1FF;
	if ((spriteLeft >> 8) & 0b1)
		spriteLeft |= 0xFFFFFF00;	//not sure maybe sign extension is okay
	if (spriteLeft >= 240 || spriteTop > VCOUNT)	//nope. sprite is offscreen or too low
		return;
	int spriteBottom = 0, spriteRight = 0;
	int rowPitch = 1;	//find out how many lines we have to 'cross' to get to next row (in 1d mapping)
	//need to find out dimensions first to figure out whether to ignore this object
	int shape = ((attr0 >> 14) & 0b11);
	int size = ((attr1 >> 14) & 0b11);
	int spritePriority = ((attr2 >> 10) & 0b11);	//used to figure out whether we should actually render the sprite

	int spriteBoundsLookupId = (shape << 2) | size;
	int spriteXBoundsLUT[12] = { 8,16,32,64,16,32,32,64,8,8,16,32 };
	int spriteYBoundsLUT[12] = { 8,16,32,64,8,8,16,32,16,32,32,64 };
	int xPitchLUT[12] = { 1,2,4,8,2,4,4,8,1,1,2,4 };

	spriteRight = spriteLeft + spriteXBoundsLUT[spriteBoundsLookupId];
	spriteBottom = spriteTop + spriteYBoundsLUT[spriteBoundsLookupId];
	rowPitch = xPitchLUT[spriteBoundsLookupId];
	int doubleSizeOffset = ((spriteBottom - spriteTop)) * doubleSize;
	if (VCOUNT >= (spriteBottom+doubleSizeOffset))	//nope, we're past it.
		return;

	uint32_t tileId = ((attr2) & 0x3FF);
	uint8_t priorityBits = ((attr2 >> 10) & 0b11);
	uint8_t paletteNumber = ((attr2 >> 12) & 0xF);
	bool hiColor = ((attr0 >> 13) & 0b1);
	if (hiColor)
		rowPitch *= 2;
	int yOffsetIntoSprite = VCOUNT - spriteTop;
	int xBase = 0;

	int halfWidth = (spriteRight - spriteLeft) / 2;
	int halfHeight = (spriteBottom - spriteTop) / 2;
	int spriteWidth = halfWidth * 2;
	int spriteHeight = halfHeight * 2;	//find out how big sprite is

	//get affine parameters
	uint32_t parameterSelection = (attr1 >> 9) & 0x1F;
	parameterSelection *= 0x20;
	parameterSelection += 6;
	int16_t PA = m_mem->OAM[parameterSelection] | ((m_mem->OAM[parameterSelection + 1]) << 8);
	int16_t PB = m_mem->OAM[parameterSelection + 8] | ((m_mem->OAM[parameterSelection + 9]) << 8);
	int16_t PC = m_mem->OAM[parameterSelection + 16] | ((m_mem->OAM[parameterSelection + 17]) << 8);
	int16_t PD = m_mem->OAM[parameterSelection + 24] | ((m_mem->OAM[parameterSelection + 25]) << 8);
	for (int x = 0; x < spriteWidth * ((doubleSize)?2:1); x++)
	{
		int ix = (x - halfWidth);
		int iy = (yOffsetIntoSprite - halfHeight);
		if (doubleSize)
		{
			ix = (x - spriteWidth);
			iy = (yOffsetIntoSprite - spriteHeight);
		}

		uint32_t px = ((PA * ix + PB * iy) >> 8);
		uint32_t py = ((PC * ix + PD * iy) >> 8);
		px += halfWidth; py += halfHeight;	//not sure about this bit, but..
		if (py >= spriteHeight || px >= spriteWidth)
			continue;

		uint32_t baseTileId = tileId;
		uint32_t yCorrection = 0;
		uint32_t yOffs = py;
		while (yOffs >= 8)
		{
			yOffs -= 8;
			if (!oneDimensionalMapping)
				baseTileId += 32;	//add 32 to get to next tile row with 2d mapping
			else
				baseTileId += rowPitch; //otherwise, add the row pitch (which says how many tiles exist per row)
		}
		uint32_t objBaseAddress = 0x10000 + (baseTileId * 32);
		yCorrection = (yOffs * ((hiColor) ? 8 : 4));


		int curXSpanTile = px /8;
		int baseX = px%8;
		uint32_t tileMapLookupAddr = objBaseAddress + yCorrection + (curXSpanTile * ((hiColor) ? 64 : 32));

		int plotCoord = x + spriteLeft;
		if (plotCoord > 239 || plotCoord < 0)
			continue;

		if (!getPointDrawable(plotCoord, VCOUNT, 0, true) && !isObjWindow)	//not sure about the 'isObjWindow' check
			continue;

		//let's see if a bg pixel with higher priority already exists! (todo: check for sprite priority too)
		uint8_t priorityAtPixel = m_spritePriorities[plotCoord];
		if ((spritePriority > priorityAtPixel) && !isObjWindow)
			continue;

		int paletteAddr = 0x200;
		if (!hiColor)
		{
			uint8_t tileData = m_mem->VRAM[tileMapLookupAddr + (baseX / 2)];
			int paletteId = tileData & 0xF;
			if (baseX & 0b1)
				paletteId = ((tileData >> 4) & 0xF);

			if (!paletteId)		//don't render if palette id == 0
				continue;

			paletteAddr += ((int)(paletteNumber) * 32) + (paletteId * 2);
		}
		else
		{
			uint8_t tileData = m_mem->VRAM[tileMapLookupAddr + baseX];
			int paletteId = tileData;
			if (!paletteId)
				continue;
			paletteAddr += (paletteId * 2);
		}
		uint8_t colLow = m_mem->paletteRAM[paletteAddr];
		uint8_t colHigh = m_mem->paletteRAM[paletteAddr + 1];
		uint16_t col = ((colHigh << 8) | colLow);

		if (isObjWindow)
			m_objWindowMask[plotCoord] = 1;
		else
		{
			m_spritePriorities[plotCoord] = spritePriority;

			if (getPointBlendable(plotCoord, VCOUNT))
			{
				if (isBlendTarget2)
					target2Pixels[plotCoord] = col;
				if (isBlendTarget1)
				{
					switch (blendMode)
					{
					case 2:
						col = blendBrightness(col, true);
						break;
					case 3:
						col = blendBrightness(col, false);
						break;
					}
				}
			}

			uint32_t res = col16to32(col);
			m_spriteLineBuffer[plotCoord] = res;
		}
	}

}

uint16_t PPU::blendBrightness(uint16_t col, bool increase)
{
	uint8_t red = (col & 0x1F);
	uint8_t green = (col >> 5) & 0x1F;
	uint8_t blue = (col >> 10) & 0x1F;

	uint8_t bldCoefficient = min(16,BLDY & 0x1F);

	if (increase)
	{
		red = red + (((31 - red) * bldCoefficient) / 16);
		green = green + (((31 - green) * bldCoefficient) / 16);
		blue = blue + (((31 - blue) * bldCoefficient) / 16);
	}
	else
	{
		red = red - ((red * bldCoefficient) / 16);
		green = green - ((green * bldCoefficient) / 16);
		blue = blue - ((blue * bldCoefficient) / 16);
	}

	uint16_t res = (red & 0x1F) | ((green & 0x1F) << 5) | ((blue & 0x1F) << 10);
	return res;
}

uint16_t PPU::blendAlpha(uint16_t colA, uint16_t colB)
{
	//maybe a better way to do this. :P
	uint8_t redA = (colA & 0x1F);
	uint8_t greenA = (colA >> 5) & 0x1F;
	uint8_t blueA = (colA >> 10) & 0x1F;
	uint8_t bldCoeffA = min(16,BLDALPHA & 0x1F);
	redA = (redA * bldCoeffA) / 16;
	greenA = (greenA * bldCoeffA) / 16;
	blueA = (blueA * bldCoeffA) / 16;

	uint8_t redB = (colB & 0x1F);
	uint8_t greenB = (colB >> 5) & 0x1F;
	uint8_t blueB = (colB >> 10) & 0x1F;
	uint8_t bldCoeffB = min(16,((BLDALPHA >> 8) & 0x1F));
	redB = (redB * bldCoeffB) / 16;
	greenB = (greenB * bldCoeffB) / 16;
	blueB = (blueB * bldCoeffB) / 16;

	redA = min(31, (redA + redB));
	greenA = min(31, (greenA + greenB));
	blueA = min(31, (blueA + blueB));
	return (redA & 0x1F) | ((greenA & 0x1F) << 5) | ((blueA & 0x1F) << 10);
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

bool PPU::getPointDrawable(int x, int y, int backgroundLayer, bool obj)
{
	bool window0Enabled = ((DISPCNT >> 13) & 0b1);
	bool window1Enabled = ((DISPCNT >> 14) & 0b1);
	bool objWindowEnabled = ((DISPCNT >> 15) & 0b1) && ((DISPCNT >> 12) & 0b1);
	if (!(window0Enabled || window1Enabled || objWindowEnabled))		//drawable if neither window enabled
		return true;
	bool drawable = false;
	//todo: obj window
	//also todo: window priority. win0 has higher priority than win1, win1 has higher priority than obj window
	if (window0Enabled)
	{
		int winRight = (WIN0H & 0xFF);
		int winLeft = ((WIN0H >> 8) & 0xFF);
		int winBottom = (WIN0V & 0xFF) - 1;	//not sure about -1? gbateek says bottom-most plus 1
		int winTop = ((WIN0V >> 8) & 0xFF);
		bool inWindow = (x >= winLeft && x <= winRight && y >= winTop && y <= winBottom);
		if (inWindow)
		{
			if (obj)
				drawable = ((WININ >> 4) & 0b1);
			else
				drawable = ((WININ >> backgroundLayer) & 0b1);
		}
		else
		{
			if (obj)
				drawable = ((WINOUT >> 4) & 0b1);
			else
				drawable = ((WINOUT >> backgroundLayer) & 0b1);
		}
	}
	if (window1Enabled)
	{
		int winRight = (WIN1H & 0xFF);
		int winLeft = ((WIN1H >> 8) & 0xFF);
		int winBottom = (WIN1V & 0xFF) - 1;	//not sure about -1? gbateek says bottom-most plus 1
		int winTop = ((WIN1V >> 8) & 0xFF);
		bool inWindow = (x >= winLeft && x <= winRight && y >= winTop && y <= winBottom);
		if (inWindow)
		{
			if (obj)
				drawable |= ((WININ >> 12) & 0b1);
			else
				drawable |= ((WININ >> (backgroundLayer+8)) & 0b1);
		}
		else
		{
			if (obj)
				drawable |= ((WINOUT >> 4) & 0b1);
			else
				drawable |= ((WINOUT >> (backgroundLayer)) & 0b1);
		}
	}
	if (objWindowEnabled)
	{
		bool pointInWindow = m_objWindowMask[x];	//<--why does vs care about this line? x can never be above 239.
		if (pointInWindow)
		{
			if (obj)
				drawable |= ((WINOUT >> 12) & 0b1);
			else
				drawable |= ((WINOUT >> (backgroundLayer + 8)) & 0b1);
		}
		else
		{
			if (obj)
				drawable |= ((WINOUT >> 4) & 0b1);
			else
				drawable |= ((WINOUT >> (backgroundLayer)) & 0b1);
		}
	}
	return drawable;
}

bool PPU::getPointBlendable(int x, int y)
{
	bool window0Enabled = ((DISPCNT >> 13) & 0b1);
	bool window1Enabled = ((DISPCNT >> 14) & 0b1);
	bool objWindowEnabled = ((DISPCNT >> 15) & 0b1) && ((DISPCNT >> 12) & 0b1);
	if (!(window0Enabled || window1Enabled || objWindowEnabled))		//drawable if neither window enabled
		return true;
	bool drawable = false;

	if (window0Enabled)
	{
		int winRight = (WIN0H & 0xFF);
		int winLeft = ((WIN0H >> 8) & 0xFF);
		int winBottom = (WIN0V & 0xFF) - 1;	//not sure about -1? gbateek says bottom-most plus 1
		int winTop = ((WIN0V >> 8) & 0xFF);
		bool inWindow = (x >= winLeft && x <= winRight && y >= winTop && y <= winBottom);
		if (inWindow)
			drawable = ((WININ >> 5) & 0b1);
		else
			drawable = ((WINOUT >> 5) & 0b1);
	}
	if (window1Enabled)
	{
		int winRight = (WIN1H & 0xFF);
		int winLeft = ((WIN1H >> 8) & 0xFF);
		int winBottom = (WIN1V & 0xFF) - 1;	//not sure about -1? gbateek says bottom-most plus 1
		int winTop = ((WIN1V >> 8) & 0xFF);
		bool inWindow = (x >= winLeft && x <= winRight && y >= winTop && y <= winBottom);
		if (inWindow)
			drawable |= ((WININ >> 13) & 0b1);
		else
			drawable |= ((WINOUT >> 5) & 0b1);
	}
	if (objWindowEnabled)
	{
		bool pointInWindow = m_objWindowMask[x];	//<--why does vs care about this line? x can never be above 239.
		if (pointInWindow)
			drawable |= ((WINOUT >> 13) & 0b1);
		else
			drawable |= ((WINOUT >> 5) & 0b1);
	}
	return drawable;

}

void PPU::updateAffineRegisters(int bg)
{
	if (bg == 2)
	{
		int16_t dmx = BG2PB;
		int16_t dmy = BG2PD;

		if ((BG2X >> 27) & 0b1)
			BG2X |= 0xF0000000;
		if ((BG2Y >> 27) & 0b1)
			BG2Y |= 0xF0000000;

		BG2X = (BG2X + dmx) & 0xFFFFFFF;
		BG2Y = (BG2Y + dmy) & 0xFFFFFFF;
	}
	if (bg == 3)
	{
		int16_t dmx = BG3PB;
		int16_t dmy = BG3PD;

		if ((BG3X >> 27) & 0b1)
			BG3X |= 0xF0000000;
		if ((BG3Y >> 27) & 0b1)
			BG3Y |= 0xF0000000;

		BG3X = (BG3X + dmx) & 0xFFFFFFF;
		BG3Y = (BG3Y + dmy) & 0xFFFFFFF;
	}
}

uint32_t* PPU::getDisplayBuffer()
{
	return m_renderBuffer[!pageIdx];
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
	case 0x04000048:
		return WININ & 0xFF;
	case 0x04000049:
		return ((WININ >> 8) & 0xFF);
	case 0x0400004A:
		return WINOUT & 0xFF;
	case 0x0400004B:
		return ((WINOUT >> 8) & 0xFF);
	case 0x04000050:
		return BLDCNT & 0xFF;
	case 0x04000051:
		return ((BLDCNT >> 8) & 0xFF);
	case 0x04000052:
		return BLDALPHA & 0xFF;
	case 0x04000053:
		return ((BLDALPHA >> 8) & 0xFF);
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
		BG0HOFS &= 0xFF; BG0HOFS |= (((value&0b1) << 8));
		break;
	case 0x04000012:
		BG0VOFS &= 0xFF00; BG0VOFS |= value;
		break;
	case 0x04000013:
		BG0VOFS &= 0xFF; BG0VOFS |= (((value&0b1) << 8));
		break;
	case 0x04000014:
		BG1HOFS &= 0xFF00; BG1HOFS |= value;
		break;
	case 0x04000015:
		BG1HOFS &= 0xFF; BG1HOFS |= (((value&0b1) << 8));
		break;
	case 0x04000016:
		BG1VOFS &= 0xFF00; BG1VOFS |= value;
		break;
	case 0x04000017:
		BG1VOFS &= 0xFF; BG1VOFS |= (((value&0b1) << 8));
		break;
	case 0x04000018:
		BG2HOFS &= 0xFF00; BG2HOFS |= value;
		break;
	case 0x04000019:
		BG2HOFS &= 0xFF; BG2HOFS |= (((value&0b1) << 8));
		break;
	case 0x0400001A:
		BG2VOFS &= 0xFF00; BG2VOFS |= value;
		break;
	case 0x0400001B:
		BG2VOFS &= 0xFF; BG2VOFS |= (((value&0b1) << 8));
		break;
	case 0x0400001C:
		BG3HOFS &= 0xFF00; BG3HOFS |= value;
		break;
	case 0x0400001D:
		BG3HOFS &= 0xFF; BG3HOFS |= (((value&0b1) << 8));
		break;
	case 0x0400001E:
		BG3VOFS &= 0xFF00; BG3VOFS |= value;
		break;
	case 0x0400001F:
		BG3VOFS &= 0xFF; BG3VOFS |= (((value&0b1) << 8));
		break;
	case 0x04000040:
		WIN0H &= 0xFF00; WIN0H |= value;
		break;
	case 0x04000041:
		WIN0H &= 0xFF; WIN0H |= (value << 8);
		break;
	case 0x04000042:
		WIN1H &= 0xFF00; WIN1H |= value;
		break;
	case 0x04000043:
		WIN1H &= 0xFF; WIN1H |= ((value << 8));
		break;
	case 0x04000044:
		WIN0V &= 0xFF00; WIN0V |= value;
		break;
	case 0x04000045:
		WIN0V &= 0xFF; WIN0V |= (value << 8);
		break;
	case 0x04000046:
		WIN1V &= 0xFF00; WIN1V |= value;
		break;
	case 0x04000047:
		WIN1V &= 0xFF; WIN1V |= (value << 8);
		break;
	case 0x04000048:
		WININ &= 0xFF00; WININ |= value;
		break;
	case 0x04000049:
		WININ &= 0xFF; WININ |= (value << 8);
		break;
	case 0x0400004A:
		WINOUT &= 0xFF00; WINOUT |= value;
		break;
	case 0x0400004B:
		WINOUT &= 0xFF; WINOUT |= (value << 8);
		break;
	case 0x04000020:
		BG2PA &= 0xFF00; BG2PA |= value;
		break;
	case 0x04000021:
		BG2PA &= 0xFF; BG2PA |= (value << 8);
		break;
	case 0x04000022:
		BG2PB &= 0xFF00; BG2PB |= value;
		break;
	case 0x04000023:
		BG2PB &= 0xFF; BG2PB |= (value << 8);
		break;
	case 0x04000024:
		BG2PC &= 0xFF00; BG2PC |= value;
		break;
	case 0x04000025:
		BG2PC &= 0xFF; BG2PC |= (value << 8);
		break;
	case 0x04000026:
		BG2PD &= 0xFF00; BG2PD |= value;
		break;
	case 0x04000027:
		BG2PD &= 0xFF; BG2PD |= (value << 8);
		break;
	case 0x04000028:
		BG2X_latch &= 0xFFFFFF00; BG2X_latch |= value;
		break;
	case 0x04000029:
		BG2X_latch &= 0xFFFF00FF; BG2X_latch |= (value << 8);
		break;
	case 0x0400002A:
		BG2X_latch &= 0xFF00FFFF; BG2X_latch |= (value << 16);
		break;
	case 0x0400002B:
		BG2X_latch &= 0x00FFFFFF; BG2X_latch |= (value << 24);
		BG2X = BG2X_latch;
		break;
	case 0x0400002C:
		BG2Y_latch &= 0xFFFFFF00; BG2Y_latch |= value;
		break;
	case 0x0400002D:
		BG2Y_latch &= 0xFFFF00FF; BG2Y_latch |= (value << 8);
		break;
	case 0x0400002E:
		BG2Y_latch &= 0xFF00FFFF; BG2Y_latch |= (value << 16);
		break;
	case 0x0400002F:
		BG2Y_latch &= 0x00FFFFFF; BG2Y_latch |= (value << 24);
		BG2Y = BG2Y_latch;
		break;
	case 0x04000030:
		BG3PA &= 0xFF00; BG3PA |= value;
		break;
	case 0x04000031:
		BG3PA &= 0xFF; BG3PA |= (value << 8);
		break;
	case 0x04000032:
		BG3PB &= 0xFF00; BG3PB |= value;
		break;
	case 0x04000033:
		BG3PB &= 0xFF; BG3PB |= (value << 8);
		break;
	case 0x04000034:
		BG3PC &= 0xFF00; BG3PC |= value;
		break;
	case 0x04000035:
		BG3PC &= 0xFF; BG3PC |= (value << 8);
		break;
	case 0x04000036:
		BG3PD &= 0xFF00; BG3PD |= value;
		break;
	case 0x04000037:
		BG3PD &= 0xFF; BG3PD |= (value << 8);
		break;
	case 0x04000038:
		BG3X_latch &= 0xFFFFFF00; BG3X_latch |= value;
		break;
	case 0x04000039:
		BG3X_latch &= 0xFFFF00FF; BG3X_latch |= (value << 8);
		break;
	case 0x0400003A:
		BG3X_latch &= 0xFF00FFFF; BG3X_latch |= (value << 16);
		break;
	case 0x0400003B:
		BG3X_latch &= 0x00FFFFFF; BG3X_latch |= (value << 24);
		BG3X = BG3X_latch;
		break;
	case 0x0400003C:
		BG3Y_latch &= 0xFFFFFF00; BG3Y_latch |= value;
		break;
	case 0x0400003D:
		BG3Y_latch &= 0xFFFF00FF; BG3Y_latch |= (value << 8);
		break;
	case 0x0400003E:
		BG3Y_latch &= 0xFF00FFFF; BG3Y_latch |= (value << 16);
		break;
	case 0x0400003F:
		BG3Y_latch &= 0x00FFFFFF; BG3Y_latch |= (value << 24);
		BG3Y = BG3Y_latch;
		break;
	case 0x04000050:
		BLDCNT &= 0xFF00; BLDCNT |= value;
		break;
	case 0x04000051:
		BLDCNT &= 0xFF; BLDCNT |= (value << 8);
		break;
	case 0x04000052:
		BLDALPHA &= 0xFF00; BLDALPHA |= value;
		break;
	case 0x04000053:
		BLDALPHA &= 0xFF; BLDALPHA |= (value << 8);
		break;
	case 0x04000054:
		BLDY = value;
		break;
	default:
		break;
		//Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unknown PPU IO register write {:#x}", address));
	}
}

void PPU::registerDMACallbacks(callbackFn HBlankCallback, callbackFn VBlankCallback, void* ctx)
{
	DMAHBlankCallback = HBlankCallback;
	DMAVBlankCallback = VBlankCallback;
	callbackContext = ctx;
}

bool PPU::getShouldSync()
{
	bool res = shouldSyncVideo;
	shouldSyncVideo = false;
	return res;
}

void PPU::onSchedulerEvent(void* context)
{
	PPU* thisPtr = (PPU*)context;
	thisPtr->eventHandler();
}