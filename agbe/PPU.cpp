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
	uint64_t schedTimestamp = m_scheduler->getEventTime();

	switch (m_state)
	{
	case PPUState::HDraw:
		HDraw();
		m_state = PPUState::HBlank;
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, schedTimestamp+46);
		break;
	case PPUState::HBlank:
		HBlank();
		break;
	case PPUState::VBlank:
		VBlank();
		break;
	}
}

void PPU::triggerHBlankIRQ()
{
	m_interruptManager->requestInterrupt(InterruptType::HBlank);
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
		renderMode5();
		break;
		}

	composeLayers();
	m_backgroundLayers[0].enabled = false;
	m_backgroundLayers[1].enabled = false;
	m_backgroundLayers[2].enabled = false;
	m_backgroundLayers[3].enabled = false;
	affineHorizontalMosaicCounter = 0;
}

void PPU::HBlank()
{
	uint64_t schedTimestamp = m_scheduler->getEventTime();

	if (!hblank_flagSet)	//hblank set and dma ~cycle 1006
	{
		if (((DISPSTAT >> 4) & 0b1))
			m_scheduler->addEvent(Event::HBlankIRQ, &PPU::onHBlankIRQEvent, (void*)this, schedTimestamp + 4);
		DMAHBlankCallback(callbackContext);
		if (VCOUNT >= 2)
			DMAVideoCaptureCallback(callbackContext);

		hblank_flagSet = true;
		setHBlankFlag(true);
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, schedTimestamp+226);
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
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, schedTimestamp+1006);

		DMAVBlankCallback(callbackContext);

		return;
	}
	else
		m_state = PPUState::HDraw;

	m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, schedTimestamp+960);
}

void PPU::VBlank()
{
	uint64_t schedTimestamp = m_scheduler->getEventTime();
	m_lineCycles = 0;
	m_state=PPUState::VBlank;

	if (!vblank_setHblankBit)	//handle first part of line, up until hblank
	{
		setHBlankFlag(true);

		if (((DISPSTAT >> 4) & 0b1))
			m_scheduler->addEvent(Event::HBlankIRQ, &PPU::onHBlankIRQEvent, (void*)this, schedTimestamp + 4);

		vblank_setHblankBit = true;
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, schedTimestamp+226);
		if (VCOUNT < 162)
			DMAVideoCaptureCallback(callbackContext);
		return;
	}

	vblank_setHblankBit = false;	//end of vblank for current line
	setHBlankFlag(false);
	VCOUNT++;
	if (VCOUNT == 228)		//go back to drawing
	{
		affineVerticalMosaicCounter = 0;
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
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, schedTimestamp+960);
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
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, schedTimestamp+1006);
	}

}

void PPU::renderMode0()
{
	drawSprites();
	for (int i = 0; i < 4; i++)	//todo: optimise. can use a single for loop and get each pixel one by one
	{
		if ((DISPCNT >> (8 + i)) & 0b1)
			drawBackground(i);
	}

}

void PPU::renderMode1()
{
	drawSprites();
	for (int i = 0; i < 3; i++)	//todo: optimise. can use a single for loop and get each pixel one by one
	{
		if ((DISPCNT>>(8+i))&0b1)
		{
			if (i==2)
				drawRotationScalingBackground(i);
			else
				drawBackground(i);
		}
	}
}

void PPU::renderMode2()
{
	drawSprites();
	for (int i = 2; i < 4; i++)
	{
		if ((DISPCNT >> (8 + i)) & 0b1)
			drawRotationScalingBackground(i);
	}
}

void PPU::renderMode3()
{
	drawSprites(true);
	bool mosaic = ((BG2CNT >> 6) & 0b1);
	if ((DISPCNT >> 10) & 0b1)
	{
		int32_t xRef = BG2X & 0xFFFFFFF;
		if ((xRef >> 27) & 0b1)
			xRef |= 0xF0000000;
		int32_t yRef = BG2Y & 0xFFFFFFF;
		if ((yRef >> 27) & 0b1)
			yRef |= 0xF000000;

		int16_t pA = (int16_t)BG2PA;
		int16_t pC = (int16_t)BG2PC;

		m_backgroundLayers[2].enabled = true;
		m_backgroundLayers[2].priorityBits = BG2CNT & 0b11;
		for (int i = 0; i < 240; i++, m_calcAffineCoords(mosaic,xRef,yRef,pA,pC))
		{
			uint32_t xCoord = (xRef >> 8) & 0xFFFFF;
			uint32_t yCoord = (yRef >> 8) & 0xFFFFF;
			if (xCoord > 239 || yCoord > 159)
			{
				m_backgroundLayers[2].lineBuffer[i] = 0x8000;
				continue;
			}
			uint32_t address = (yCoord * 480) + (xCoord * 2);
			uint8_t colLow = m_mem->VRAM[address];
			uint8_t colHigh = m_mem->VRAM[address + 1];
			uint16_t col = ((colHigh << 8) | colLow);
			m_backgroundLayers[2].lineBuffer[i] = col & 0x7FFF;
		}
	}
	m_updateAffineRegisters(mosaic,2);
}

void PPU::renderMode4()
{
	drawSprites(true);
	uint32_t base = 0;
	bool pageFlip = ((DISPCNT >> 4) & 0b1);
	if (pageFlip)
		base = 0xA000;
	bool mosaic = ((BG2CNT >> 6) & 0b1);
	if ((DISPCNT >> 10) & 0b1)
	{
		int32_t xRef = BG2X & 0xFFFFFFF;
		if ((xRef >> 27) & 0b1)
			xRef |= 0xF0000000;
		int32_t yRef = BG2Y & 0xFFFFFFF;
		if ((yRef >> 27) & 0b1)
			yRef |= 0xF000000;

		int16_t pA = (int16_t)BG2PA;
		int16_t pC = (int16_t)BG2PC;
		m_backgroundLayers[2].enabled = true;
		m_backgroundLayers[2].priorityBits = BG2CNT & 0b11;
		for (int i = 0; i < 240; i++, m_calcAffineCoords(mosaic,xRef,yRef,pA,pC))
		{
			uint32_t xCoord = (xRef >> 8) & 0xFFFFF;
			uint32_t yCoord = (yRef >> 8) & 0xFFFFF;
			if (xCoord > 239 || yCoord > 159)
			{
				m_backgroundLayers[2].lineBuffer[i] = 0x8000;
				continue;
			}
			uint32_t address = base + (yCoord * 240) + xCoord;
			uint8_t curPaletteIdx = m_mem->VRAM[address];
			uint16_t paletteAddress = (uint16_t)curPaletteIdx * 2;
			uint8_t paletteLow = m_mem->paletteRAM[paletteAddress];
			uint8_t paletteHigh = m_mem->paletteRAM[paletteAddress + 1];

			uint16_t paletteData = ((paletteHigh << 8) | paletteLow);
			m_backgroundLayers[2].lineBuffer[i] = paletteData & 0x7FFF;
		}
	}
	m_updateAffineRegisters(mosaic,2);
}

void PPU::renderMode5()
{
	drawSprites(true);
	uint32_t baseAddr = 0;
	bool pageFlip = ((DISPCNT >> 4) & 0b1);
	if (pageFlip)
		baseAddr = 0xA000;
	bool mosaic = ((BG2CNT >> 6) & 0b1);
	if ((DISPCNT >> 10) & 0b1)
	{
		int32_t xRef = BG2X & 0xFFFFFFF;
		if ((xRef >> 27) & 0b1)
			xRef |= 0xF0000000;
		int32_t yRef = BG2Y & 0xFFFFFFF;
		if ((yRef >> 27) & 0b1)
			yRef |= 0xF000000;

		int16_t pA = (int16_t)BG2PA;
		int16_t pC = (int16_t)BG2PC;

		m_backgroundLayers[2].enabled = true;
		m_backgroundLayers[2].priorityBits = BG2CNT & 0b11;
		for (int i = 0; i < 240; i++, m_calcAffineCoords(mosaic,xRef,yRef,pA,pC))
		{
			uint32_t xCoord = (xRef >> 8) & 0xFFFFF;
			uint32_t yCoord = (yRef >> 8) & 0xFFFFF;
			if (xCoord > 159 || yCoord > 127)
			{
				m_backgroundLayers[2].lineBuffer[i] = 0x8000;
				continue;
			}

			uint32_t address = baseAddr + (yCoord * 320) + (xCoord * 2);
			uint8_t colLow = m_mem->VRAM[address];
			uint8_t colHigh = m_mem->VRAM[address + 1];
			uint16_t col = (colHigh << 8) | colLow;
			m_backgroundLayers[2].lineBuffer[i] = col & 0x7FFF;
		}
	}
	m_updateAffineRegisters(mosaic,2);
}

void PPU::composeLayers()
{
	uint16_t backDrop = *(uint16_t*)m_mem->paletteRAM & 0x7FFF;
	int spriteMosaicHorizontal = ((MOSAIC >> 8) & 0xF) + 1;
	int spriteHorizontalMosaicCounter = 0;
	int spriteMosaicX = 0;
	bool mosaicInProcess = false;
	for (int x = 0; x < 240; x++)
	{	
		if (((DISPCNT >> 7) & 0b1))				//forced blank -> white screen
		{
			m_renderBuffer[pageIdx][(240 * VCOUNT) + x] = 0xFFFFFFFF;
			continue;
		}

		PointRenderableInfo pointInfo = getPointDrawable(x, VCOUNT);

		uint16_t finalCol = backDrop;
		bool transparentSpriteTop = false;
		uint16_t blendPixelB = 0x8000;
		if (((BLDCNT >> 13) & 0b1))
			blendPixelB = backDrop;
		uint16_t blendPixelA = 0x8000;
		if (((BLDCNT >> 5) & 0b1))
			blendPixelA = backDrop;

		int highestBlendBPrio = 255, highestBlendAPrio = -1, blendALayer = 255;

		int highestPriority = 255;
		for (int layer = 3; layer >= 0; layer--)
		{
			if (m_backgroundLayers[layer].enabled && pointInfo.layerDrawable[layer])	//layer activated
			{
				uint16_t colAtLayer = m_backgroundLayers[layer].lineBuffer[x];
				if (!((colAtLayer >> 15) & 0b1))
				{
					bool isBlendTargetA = ((BLDCNT >> layer) & 0b1);
					if ((m_backgroundLayers[layer].priorityBits <= highestPriority))
					{
						highestPriority = m_backgroundLayers[layer].priorityBits;
						finalCol = colAtLayer;
						blendPixelA = 0x8000;					//blend target A must be top visible layer, so if this isn't a blend target it gets disabled
						highestBlendAPrio = -1;
						blendALayer = 255;
						if ((BLDCNT >> layer) & 0b1)
						{
							highestBlendAPrio = highestPriority;
							blendPixelA = finalCol;
							blendALayer = layer;
						}

					}

					if ((m_backgroundLayers[layer].priorityBits <= highestBlendBPrio) && (blendALayer!=layer))
					{
						blendPixelB = 0x8000;
						if ((BLDCNT >> (layer + 8)) & 0b1)
						{
							highestBlendBPrio = m_backgroundLayers[layer].priorityBits;
							blendPixelB = colAtLayer;
						}
					}
				}

			}
		}
		
		if (((DISPCNT >> 12) & 0b1) && pointInfo.objDrawable)
		{
			//sprite mosaic seems to be a post process thing? idk
			int spriteX = x;
			if (m_spriteAttrBuffer[x].mosaic || (!m_spriteAttrBuffer[x].mosaic && m_spriteAttrBuffer[x].priority == 0x1F && mosaicInProcess))	//second bit is dumb hack :D
			{
				mosaicInProcess = true;
				spriteX = spriteMosaicX;
			}
			uint16_t spritePixel = m_spriteLineBuffer[spriteX];
			if (!((spritePixel >> 15) & 0b1) && m_spriteAttrBuffer[spriteX].priority != 0x1F)
			{
				bool isBlendTargetA = (((BLDCNT >> 4) & 0b1) || m_spriteAttrBuffer[spriteX].transparent);
				if (m_spriteAttrBuffer[spriteX].priority <= highestPriority)
				{
					finalCol = spritePixel;
					if (isBlendTargetA)
					{
						//weird!! blend target b might not have been selected if the topmost layer before was A+B, so make sure we set it now
						if (blendALayer <= 3 && ((BLDCNT >> (8 + blendALayer)) & 0b1))
						{
							blendPixelB = blendPixelA;
							highestBlendBPrio = highestBlendAPrio;
						}

						transparentSpriteTop = m_spriteAttrBuffer[spriteX].transparent;
						blendPixelA = finalCol;
						blendALayer = 255;
					}
					else
					{
						blendPixelA = 0x8000;
						highestBlendAPrio = -1;
					}
				}
				//only choose as target b if not target a (i.e. not transparent), and not topmost visible layer
				if (m_spriteAttrBuffer[x].priority > highestPriority && m_spriteAttrBuffer[x].priority <= highestBlendBPrio)
				{
					blendPixelB = 0x8000;
					if (((BLDCNT >> 12) & 0b1))
						blendPixelB = spritePixel;
				}
			}
		}

		if (pointInfo.blendable)
		{
			uint8_t blendMode = ((BLDCNT >> 6) & 0b11);
			if (transparentSpriteTop)	
			{
				if (!(blendPixelB >> 15))									//i think blend mode only gets overridden if there's a second target pixel?
					blendMode = 1;
				else if (!((BLDCNT >> 4) & 0b1) && (blendPixelB >> 15))		//hmm, if it's only 'semi-transparent' but there's no target B (and sprite not target A), then don't blend??
					blendPixelA = 0x8000;
			}
			switch (blendMode)
			{
			case 1:
				if(!(blendPixelA >> 15) && !(blendPixelB >> 15))
					finalCol = blendAlpha(blendPixelA, blendPixelB);
				break;
			case 2:
				if (!(blendPixelA>>15))
					finalCol = blendBrightness(blendPixelA, true);
				break;
			case 3:
				if (!(blendPixelA>>15))
					finalCol = blendBrightness(blendPixelA, false);
				break;
			}
		}

		m_renderBuffer[pageIdx][(240 * VCOUNT) + x] = col16to32(finalCol);

		spriteHorizontalMosaicCounter++;
		if (spriteHorizontalMosaicCounter == spriteMosaicHorizontal)
		{
			spriteHorizontalMosaicCounter = 0;
			spriteMosaicX += spriteMosaicHorizontal;
			//reset when mosaic x increases, to stop sprite rendering for too many lines potentially
			mosaicInProcess = false;
		}
	}
}

void PPU::drawBackground(int bg)
{
	uint16_t ctrlReg = 0;
	uint32_t xScroll = 0, yScroll = 0;
	bool isTarget1 = ((BLDCNT >> bg) & 0b1);
	bool isTarget2 = ((BLDCNT >> (bg + 8)) & 0b1);
	int mosaicHorizontal = (MOSAIC & 0xF) + 1;
	int mosaicVertical = ((MOSAIC >> 4) & 0xF) + 1;
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

	bool mosaicEnabled = ((ctrlReg >> 6) & 0b1);

	uint8_t bgPriority = ctrlReg & 0b11;
	uint32_t tileDataBaseBlock = ((ctrlReg >> 2) & 0b11);
	bool hiColor = ((ctrlReg >> 7) & 0b1);
	uint32_t bgMapBaseBlock = ((ctrlReg >> 8) & 0x1F);
	int screenSize = ((ctrlReg >> 14) & 0b11);
	int xSizeLut[4] = { 255,511,255,511 };
	int ySizeLut[4] = { 255,255,511,511 };
	int xWrap = xSizeLut[screenSize];
	int yWrap = ySizeLut[screenSize];

	int y = VCOUNT;
	if (mosaicEnabled)
		y = (y / mosaicVertical) * mosaicVertical;

	uint32_t fetcherY = ((y + yScroll) & yWrap);
	if (screenSize && (fetcherY > 255))
	{
		fetcherY -= 256;
		bgMapBaseBlock += 1;	
		if (screenSize == 3)	//not completely sure
			bgMapBaseBlock += 1;
	}

	uint32_t bgMapYIdx = ((fetcherY / 8) * 32) * 2; //each row is 32 chars - each char is 2 bytes

	uint16_t cachedTile = 0;
	uint32_t cachedXOffs = 0xFFFFFFFF;

	m_backgroundLayers[bg].enabled = true;
	m_backgroundLayers[bg].priorityBits = bgPriority;

	for (int x = 0; x < 240; x++)
	{
		uint32_t plotAddr = (VCOUNT * 240) + x;
		int tempX = x;
		if (mosaicEnabled)
			tempX = (x / mosaicHorizontal) * mosaicHorizontal;
		int xCoord = (tempX + xScroll) & xWrap;
		int baseBlockOffset = 0;				//x scrolling can cause new baseblock to bee selected
		if (screenSize && (xCoord > 255))
		{
			xCoord -= 256;
			baseBlockOffset += 1;
		}
		uint16_t tile = cachedTile;
		uint32_t xTileIdx = (xCoord >> 3);
		if (cachedXOffs != xTileIdx)
		{
			uint32_t bgMapBaseAddress = ((bgMapBaseBlock + baseBlockOffset) * 2048) + bgMapYIdx;
			uint32_t curBgAddr = bgMapBaseAddress + ((xTileIdx * 2));
			uint8_t tileLower = m_mem->VRAM[curBgAddr];
			uint8_t tileHigher = m_mem->VRAM[curBgAddr + 1];
			tile = (((uint16_t)tileHigher << 8) | tileLower);
			cachedTile = tile;
		}
		cachedXOffs = xTileIdx;

		uint32_t tileNumber = tile & 0x3FF;
		uint32_t paletteNumber = ((tile >> 12) & 0xF);
		bool horizontalFlip = ((tile >> 10) & 0b1);
		bool verticalFlip = ((tile >> 11) & 0b1);
		uint32_t paletteMemoryAddr = 0;
		uint32_t tileMapBaseAddress = 0;

		int yMod8 = ((fetcherY & 7));
		if (verticalFlip)
			yMod8 = 7 - yMod8;


		uint32_t tileByteSizeLUT[2] = { 32,64 };
		uint32_t tileRowPitchLUT[2] = { 4,8 };

		tileMapBaseAddress = (tileDataBaseBlock * 16384) + (tileNumber * tileByteSizeLUT[hiColor]);
		tileMapBaseAddress += (yMod8 * tileRowPitchLUT[hiColor]);
		int xmod8 = (xCoord & 7);
		if (horizontalFlip)
			xmod8 = 7 - xmod8;

		uint16_t col = extractColorFromTile(tileMapBaseAddress, xmod8, hiColor, false, paletteNumber);
		m_backgroundLayers[bg].lineBuffer[x] = col;
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
	int screenSize = ((ctrlReg >> 14) & 0b11);
	int sizeLut[4] = { 128,256,512,1024 };
	int xWrap = sizeLut[screenSize];
	int yWrap = sizeLut[screenSize];

	uint8_t bgPriority = ctrlReg & 0b11;
	uint32_t tileDataBaseBlock = ((ctrlReg >> 2) & 0b11);
	uint32_t bgMapBaseBlock = ((ctrlReg >> 8) & 0x1F);
	bool overflowWrap = ((ctrlReg >> 13) & 0b1);

	uint32_t cachedTileIdx = 0;
	uint32_t cachedXCoord = 0xFFFFFFFF;
	uint32_t cachedYCoord = 0xFFFFFFFF;

	m_backgroundLayers[bg].enabled = true;
	m_backgroundLayers[bg].priorityBits = bgPriority;

	bool mosaicEnabled = (ctrlReg >> 6) & 0b1;

	for (int x = 0; x < 240; x++, m_calcAffineCoords(mosaicEnabled,xRef,yRef,pA,pC))
	{
		uint32_t plotAddr = (VCOUNT * 240) + x;
		uint32_t fetcherY = (uint32_t)((int32_t)yRef >> 8);
		if ((fetcherY >= yWrap) && !overflowWrap)
		{
			m_backgroundLayers[bg].lineBuffer[x] = 0x8000;
			continue;
		}
		fetcherY = fetcherY % yWrap;

		uint32_t xCoord = (uint32_t)((int32_t)xRef >> 8);
		if ((xCoord >= xWrap) && !overflowWrap)
		{
			m_backgroundLayers[bg].lineBuffer[x] = 0x8000;
			continue;
		}
		xCoord = xCoord % xWrap;

		uint32_t bgMapYIdx = ((fetcherY / 8) * (xWrap >> 3)); //each row is 32 chars; in rotation/scroll each entry is 1 byte
		uint32_t tileIdx = cachedTileIdx;
		if (cachedXCoord != (xCoord>>3) || cachedYCoord != bgMapYIdx)
		{
			uint32_t bgMapAddr = (bgMapBaseBlock * 2048) + bgMapYIdx;
			bgMapAddr += (xCoord>>3);
			tileIdx = m_mem->VRAM[bgMapAddr];
			cachedTileIdx = tileIdx;
		}
		cachedXCoord = (xCoord>>3);
		cachedYCoord = bgMapYIdx;

		uint32_t tileMapBaseAddress = (tileDataBaseBlock * 16384) + (tileIdx * 64);
		tileMapBaseAddress += ((fetcherY % 8) * 8);
		uint16_t col = extractColorFromTile(tileMapBaseAddress, xCoord&7, true, false, 0);

		m_backgroundLayers[bg].lineBuffer[x] = col;
	}

	m_updateAffineRegisters(mosaicEnabled,bg);
}

void PPU::drawSprites(bool bitmapMode)
{
	memset(m_spriteAttrBuffer, 0b00011111, 240);
	memset(m_spriteLineBuffer, 0x80, 480);
	m_spriteCyclesElapsed = 0;
	if (!((DISPCNT >> 12) & 0b1))
		return;
	bool oneDimensionalMapping = ((DISPCNT >> 6) & 0b1);
	bool isBlendTarget1 = ((BLDCNT >> 4) & 0b1);
	bool isBlendTarget2 = ((BLDCNT >> 12) & 0b1);
	uint8_t blendMode = ((BLDCNT >> 6) & 0b11);

	int mosaicHorizontal = ((MOSAIC >> 8) & 0xF) + 1;
	int mosaicVertical = ((MOSAIC >> 12) & 0xF) + 1;

	bool limitSpriteCycles = ((DISPCNT >> 5) & 0b1);
	int maxAllowedSpriteCycles = (limitSpriteCycles) ? 954 : 1210;	//with h-blank interval free set, then less cycles can be spent rendering sprites

	for (int i = 0; i < 128; i++)
	{
		if (m_spriteCyclesElapsed > maxAllowedSpriteCycles)	//quit sprite rendering if we've spent too much time evaluating sprites
			return;
		uint32_t spriteBase = i * 8;	//each OAM entry is 8 bytes

		OAMEntry* curSpriteEntry = (OAMEntry*)(m_mem->OAM+spriteBase);

		if(curSpriteEntry->rotateScale)
		{
			drawAffineSprite(curSpriteEntry);
			continue;
		}

		if (curSpriteEntry->disableObj)
			continue;

		//uint8_t objMode = (curSpriteEntry->attr0 >> 10) & 0b11;	
		bool isObjWindow = curSpriteEntry->objMode == 2;
		//bool mosaic = (curSpriteEntry->attr0 >> 12) & 0b1;

		int renderY = VCOUNT;
		if (curSpriteEntry->mosaic)
			renderY = (renderY / mosaicVertical) * mosaicVertical;

		int spriteTop = curSpriteEntry->yCoord;
		if (spriteTop >= 160)							//bit of a dumb hack to accommodate for when sprites are offscreen
			spriteTop -= 256;
		int spriteLeft = curSpriteEntry->xCoord;
		if (spriteLeft >= 240)
			spriteLeft -= 512;
		if (spriteLeft >= 240 || spriteTop > renderY)	//nope. sprite is offscreen or too low
			continue;
		int spriteBottom = 0, spriteRight = 0;
		int rowPitch = 1;	//find out how many lines we have to 'cross' to get to next row (in 1d mapping)
		//need to find out dimensions first to figure out whether to ignore this object
		int spritePriority = curSpriteEntry->priority;

		int spriteBoundsLookupId = (curSpriteEntry->shape << 2) | curSpriteEntry->size;
		int spriteXBoundsLUT[12] ={8,16,32,64,16,32,32,64,8,8,16,32};
		int spriteYBoundsLUT[12] ={8,16,32,64,8,8,16,32,16,32,32,64};
		int xPitchLUT[12] ={1,2,4,8,2,4,4,8,1,1,2,4};

		spriteRight = spriteLeft + spriteXBoundsLUT[spriteBoundsLookupId];
		spriteBottom = spriteTop + spriteYBoundsLUT[spriteBoundsLookupId];
		rowPitch = xPitchLUT[spriteBoundsLookupId];	
		if (VCOUNT >= spriteBottom)	//nope, we're past it.
			continue;

		bool flipHorizontal = curSpriteEntry->xFlip;
		bool flipVertical = curSpriteEntry->yFlip;

		int spriteYSize = (spriteBottom - spriteTop);	//find out how big sprite is
		int yOffsetIntoSprite = renderY - spriteTop;
		if (flipVertical)
			yOffsetIntoSprite = (spriteYSize-1) - yOffsetIntoSprite;//flip y coord we're considering

		uint32_t tileId = curSpriteEntry->charName;
		bool hiColor = curSpriteEntry->bitDepth;
		if (hiColor)
			rowPitch *= 2;

		//check y coord, then adjust tile id as necessary
		while (yOffsetIntoSprite >= 8)
		{
			yOffsetIntoSprite -= 8;
			if (!oneDimensionalMapping)
				tileId += 32;	//add 32 to get to next tile row with 2d mapping
			else
				tileId += rowPitch; //otherwise, add the row pitch (which says how many tiles exist per row)
		}

		if (bitmapMode && tileId < 512)		//bitmap mode: only tiles 512-1023 are displayable. ignore all others
			continue;

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

		//add cycles taken to evaluate sprite
		m_spriteCyclesElapsed += (spriteRight - spriteLeft);

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

				uint16_t col = extractColorFromTile(tileMapLookupAddr, baseX, hiColor, true, curSpriteEntry->paletteNumber);

				if (isObjWindow)
				{
					if(!(col>>15))
						m_spriteAttrBuffer[plotCoord].objWindow = 1;
					continue;
				}

				uint8_t priorityAtPixel = m_spriteAttrBuffer[plotCoord].priority;
				bool renderedPixelTransparent = m_spriteLineBuffer[plotCoord] >> 15;
				bool currentPixelTransparent = col >> 15;
				if ((spritePriority >= priorityAtPixel) && (!renderedPixelTransparent || currentPixelTransparent))	//keep rendering if lower priority, BUT last pixel transparent
					continue;

				m_spriteAttrBuffer[plotCoord].priority = spritePriority & 0b11111;
				m_spriteAttrBuffer[plotCoord].transparent = (curSpriteEntry->objMode == 1);
				m_spriteAttrBuffer[plotCoord].mosaic = curSpriteEntry->mosaic;
				if(!currentPixelTransparent)
					m_spriteLineBuffer[plotCoord] = col;
			}
		}

	}
}

void PPU::drawAffineSprite(OAMEntry* curSpriteEntry)
{
	bool oneDimensionalMapping = ((DISPCNT >> 6) & 0b1);
	bool isBlendTarget1 = ((BLDCNT >> 4) & 0b1);
	bool isBlendTarget2 = ((BLDCNT >> 12) & 0b1);
	uint8_t blendMode = ((BLDCNT >> 6) & 0b11);

	bool isObjWindow = (curSpriteEntry->objMode == 2);
	bool doubleSize = curSpriteEntry->disableObj;	//odd: bit 9 is 'double-size' flag with affine sprites

	int spriteTop = curSpriteEntry->yCoord;
	int spriteLeft = curSpriteEntry->xCoord;
	if ((spriteLeft >> 8) & 0b1)
		spriteLeft |= 0xFFFFFF00;	//not sure maybe sign extension is okay
	if (spriteLeft >= 240)	//nope. sprite is offscreen or too low
		return;
	int spriteBottom = 0, spriteRight = 0;
	int rowPitch = 1;	//find out how many lines we have to 'cross' to get to next row (in 1d mapping)

	//need to find out dimensions first to figure out whether to ignore this object
	int spriteBoundsLookupId = (curSpriteEntry->shape << 2) | curSpriteEntry->size;
	int spriteXBoundsLUT[12] = { 8,16,32,64,16,32,32,64,8,8,16,32 };
	int spriteYBoundsLUT[12] = { 8,16,32,64,8,8,16,32,16,32,32,64 };
	int xPitchLUT[12] = { 1,2,4,8,2,4,4,8,1,1,2,4 };

	if (spriteTop>160)			//not sure about this hack, but oh well
		spriteTop -= 256;

	if (spriteTop > VCOUNT)
		return;

	spriteRight = spriteLeft + spriteXBoundsLUT[spriteBoundsLookupId];
	spriteBottom = spriteTop + spriteYBoundsLUT[spriteBoundsLookupId];
	rowPitch = xPitchLUT[spriteBoundsLookupId];
	int doubleSizeOffset = ((spriteBottom - spriteTop)) * doubleSize;
	if (VCOUNT >= (spriteBottom+doubleSizeOffset))	//nope, we're past it.
		return;

	uint32_t tileId = curSpriteEntry->charName;
	bool hiColor = curSpriteEntry->bitDepth;
	if (hiColor)
		rowPitch *= 2;
	int yOffsetIntoSprite = VCOUNT - spriteTop;
	int xBase = 0;

	int halfWidth = (spriteRight - spriteLeft) / 2;
	int halfHeight = (spriteBottom - spriteTop) / 2;
	int spriteWidth = halfWidth * 2;
	int spriteHeight = halfHeight * 2;	//find out how big sprite is

	//add evaluation cycles. doublesize sprites take up more because twice the amount of pixels are rendered.
	m_spriteCyclesElapsed += 10;
	m_spriteCyclesElapsed += (spriteWidth * 2) * (doubleSize ? 2 : 1);

	//get affine parameters
	uint32_t parameterSelection = (curSpriteEntry->data >> 25) & 0x1F;
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
		px += halfWidth; py += halfHeight;	
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

		uint16_t col = extractColorFromTile(tileMapLookupAddr, baseX, hiColor, true, curSpriteEntry->paletteNumber);

		if (isObjWindow)
		{
			if (!(col >> 15))
				m_spriteAttrBuffer[plotCoord].objWindow = 1;
			continue;
		}

		uint8_t priorityAtPixel = m_spriteAttrBuffer[plotCoord].priority;
		bool renderedPixelTransparent = m_spriteLineBuffer[plotCoord] >> 15;
		bool currentPixelTransparent = col >> 15;
		if ((curSpriteEntry->priority >= priorityAtPixel) && (!renderedPixelTransparent || currentPixelTransparent))	//same as for normal, only stop if we're transparent (and lower priority)
			continue;																						//...or last pixel isn't transparent
		m_spriteAttrBuffer[plotCoord].priority = curSpriteEntry->priority&0b11111;
		m_spriteAttrBuffer[plotCoord].transparent = (curSpriteEntry->objMode == 1);
		m_spriteAttrBuffer[plotCoord].mosaic = curSpriteEntry->mosaic;	
		if (!currentPixelTransparent)
			m_spriteLineBuffer[plotCoord] = col;
	}

}

uint16_t PPU::extractColorFromTile(uint32_t tileBase, uint32_t xOffset, bool hiColor, bool sprite, uint32_t palette)
{
	uint16_t col = 0;
	uint32_t paletteMemoryAddr = 0;
	if (sprite)
		paletteMemoryAddr = 0x200;
	if (!hiColor)
	{
		tileBase += (xOffset/2);

		uint8_t tileData = m_mem->VRAM[tileBase];
		int colorId = 0;
		int stepTile = ((xOffset & 0b1)) << 2;
		colorId = ((tileData >> stepTile) & 0xf);	//first (even) pixel - low nibble. second (odd) pixel - high nibble
		if (!colorId)
			return 0x8000;

		paletteMemoryAddr += palette * 32;
		paletteMemoryAddr += (colorId * 2);
	}
	else
	{
		tileBase += xOffset;
		uint8_t tileData = m_mem->VRAM[tileBase];
		if (!tileData)
			return 0x8000;
		paletteMemoryAddr += (tileData * 2);
	}

	uint8_t colLow = m_mem->paletteRAM[paletteMemoryAddr];
	uint8_t colHigh = m_mem->paletteRAM[paletteMemoryAddr + 1];

	col = (colHigh << 8) | colLow;

	return col & 0x7fff;
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

PointRenderableInfo PPU::getPointDrawable(int x, int y)
{
	PointRenderableInfo info = {};
	bool window0Enabled = ((DISPCNT >> 13) & 0b1);
	bool window1Enabled = ((DISPCNT >> 14) & 0b1);
	bool objWindowEnabled = ((DISPCNT >> 15) & 0b1) && ((DISPCNT >> 12) & 0b1);
	if (!(window0Enabled || window1Enabled || objWindowEnabled))		//drawable if neither window enabled
		return { {true,true,true,true},true,true };
	//todo: obj window
	//also todo: window priority. win0 has higher priority than win1, win1 has higher priority than obj window
	if (window0Enabled)
	{
		int winRight = min((WIN0H & 0xFF)-1,239);
		int winLeft = ((WIN0H >> 8) & 0xFF);
		int winBottom = min((WIN0V & 0xFF) - 1,159);	
		int winTop = ((WIN0V >> 8) & 0xFF);
		bool inWindow = (x >= winLeft && x <= winRight && y >= winTop && y <= winBottom);
		if (inWindow)
		{
			info.objDrawable = ((WININ >> 4) & 0b1);
			for(int i = 0; i < 4; i++)
				info.layerDrawable[i] = ((WININ >> i) & 0b1);
			info.blendable = ((WININ >> 5) & 0b1);
			return info;
		}
	}
	if (window1Enabled)
	{
		int winRight = min((WIN1H & 0xFF)-1,239);
		int winLeft = ((WIN1H >> 8) & 0xFF);
		int winBottom = min((WIN1V & 0xFF) - 1,159);
		int winTop = ((WIN1V >> 8) & 0xFF);
		bool inWindow = (x >= winLeft && x <= winRight && y >= winTop && y <= winBottom);
		if (inWindow)
		{
			info.objDrawable = ((WININ >> 12) & 0b1);
			for(int i = 0; i < 4; i++)
				info.layerDrawable[i] = ((WININ >> (i + 8)) & 0b1);
			info.blendable = ((WININ >> 13) & 0b1);
			return info;
		}
	}
	if (objWindowEnabled)
	{
		bool pointInWindow = m_spriteAttrBuffer[x].objWindow;	//<--why does vs care about this line? x can never be above 239.
		if (pointInWindow)
		{
			info.objDrawable = ((WINOUT >> 12) & 0b1);
			for (int i = 0; i < 4; i++)
				info.layerDrawable[i] = ((WINOUT >> (i + 8)) & 0b1);
			info.blendable = ((WINOUT >> 13) & 0b1);
			return info;
		}
	}

	info.objDrawable = ((WINOUT >> 4) & 0b1);
	for (int i = 0; i < 4; i++)
		info.layerDrawable[i] = ((WINOUT >> i) & 0b1);
	info.blendable = ((WINOUT >> 5) & 0b1);
	return info;
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
		return ((BG0CNT >> 8) & 0xDF);
	case 0x0400000A:
		return BG1CNT & 0xFF;
	case 0x0400000B:
		return ((BG1CNT >> 8) & 0xDF);
	case 0x0400000C:
		return BG2CNT & 0xFF;
	case 0x0400000D:
		return ((BG2CNT >> 8) & 0xFF);
	case 0x0400000E:
		return BG3CNT & 0xFF;
	case 0x0400000F:
		return ((BG3CNT >> 8) & 0xFF);
	case 0x04000048:
		return WININ & 0x3F;
	case 0x04000049:
		return ((WININ >> 8) & 0x3F);
	case 0x0400004A:
		return WINOUT & 0x3F;
	case 0x0400004B:
		return ((WINOUT >> 8) & 0x3F);
	case 0x04000050:
		return BLDCNT & 0xFF;
	case 0x04000051:
		return ((BLDCNT >> 8) & 0x3F);
	case 0x04000052:
		return BLDALPHA & 0x1F;
	case 0x04000053:
		return ((BLDALPHA >> 8) & 0x1F);
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
	case 0x0400004C:
		MOSAIC &= 0xFF00; MOSAIC |= value;
		break;
	case 0x0400004D:
		MOSAIC &= 0xFF; MOSAIC |= (value << 8);
		break;
	default:
		break;
		//Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unknown PPU IO register write {:#x}", address));
	}
}

void PPU::registerDMACallbacks(callbackFn HBlankCallback, callbackFn VBlankCallback, callbackFn videoCapture, void* ctx)
{
	DMAHBlankCallback = HBlankCallback;
	DMAVBlankCallback = VBlankCallback;
	DMAVideoCaptureCallback = videoCapture;
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

void PPU::onHBlankIRQEvent(void* context)
{
	PPU* thisPtr = (PPU*)context;
	thisPtr->triggerHBlankIRQ();
}

int PPU::getVCOUNT()
{
	return VCOUNT;
}