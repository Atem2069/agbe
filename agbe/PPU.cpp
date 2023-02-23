#include"PPU.h"

PPU::PPU(std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<Scheduler> scheduler)
{
	m_scheduler = scheduler;
	m_interruptManager = interruptManager;
	reset();

	for (int i = 0; i < 4; i++)			//initially clear all fields in bg layer structs
	{
		m_windows[i] = {};
		m_backgroundLayers[i] = {};
	}
}

PPU::~PPU()
{
	if (renderThread.joinable())
		renderThread.join();
}

void PPU::reset()
{
	//reset ppu state, reschedule hdraw vcount=0
	m_scheduler->removeEvent(Event::PPU);
	m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, m_scheduler->getCurrentTimestamp() + 1006);
	m_registers.VCOUNT = 0;
	inVBlank = false;
	m_state = PPUState::HDraw;
	memset(m_renderBuffer, 0, 240 * 160 * 8);
	updateDisplayOutput();
}

void PPU::updateDisplayOutput()
{
	memcpy(m_safeDisplayBuffer, m_renderBuffer[!pageIdx], 240 * 160 * sizeof(uint32_t));
}

void PPU::registerMemory(std::shared_ptr<GBAMem> mem)
{
	registered = true;
	m_mem = mem;
	renderThread = std::thread(&PPU::drawScreen, this);
}

void PPU::eventHandler()
{
	uint64_t schedTimestamp = m_scheduler->getEventTime();

	switch (m_state)
	{
	case PPUState::HDraw:
		HDraw();
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

	//update affine registers here at end of hdraw/start of hblank
	//todo: encapsulate all ppu io registers per-scanline, pass to render thread
	uint8_t mode = m_registers.DISPCNT & 0b111;
	switch (mode)
	{
	case 1: case 3: case 4: case 5:
	{
		if ((m_registers.DISPCNT >> 10) & 0b1)
			m_updateAffineRegisters(((m_registers.BG2CNT >> 6) & 0b1), 2);
		break;
	}
	case 2:
	{
		if ((m_registers.DISPCNT >> 10) & 0b1)
			m_updateAffineRegisters(((m_registers.BG2CNT >> 6) & 0b1), 2);
		if ((m_registers.DISPCNT >> 11) & 0b1)
			m_updateAffineRegisters(((m_registers.BG3CNT >> 6) & 0b1), 2);
		break;
	}
	}


	if (((m_registers.DISPSTAT >> 4) & 0b1))
		m_scheduler->addEvent(Event::HBlankIRQ, &PPU::onHBlankIRQEvent, (void*)this, m_scheduler->getEventTime() + 4);
	DMAHBlankCallback(callbackContext);

	//timing for video capture is wrong! should fix!
	if (m_registers.VCOUNT >= 2)
		DMAVideoCaptureCallback(callbackContext);

	setHBlankFlag(true);
	m_state = PPUState::HBlank;
	m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, m_scheduler->getEventTime() + 225);
}

void PPU::HBlank()
{
	uint64_t schedTimestamp = m_scheduler->getEventTime();
	setHBlankFlag(false);
	m_lineCycles = 0;

	m_registers.VCOUNT++;

	checkVCOUNTInterrupt();

	if (m_registers.VCOUNT == 160)
	{
		renderThread.join();
		setVBlankFlag(true);
		inVBlank = true;

		if (((m_registers.DISPSTAT >> 3) & 0b1))
			m_interruptManager->requestInterrupt(InterruptType::VBlank);

		pageIdx = !pageIdx;

		m_state = PPUState::VBlank;
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, schedTimestamp+1007);

		DMAVBlankCallback(callbackContext);

		return;
	}
	else
		m_state = PPUState::HDraw;

	//see if we need to latch new vals into affine regs (e.g. if they were modified outside of vblank)
	if (BG2X_dirty)
		m_registers.BG2X_latch = m_registers.BG2X;
	if (BG2Y_dirty)
		m_registers.BG2Y_latch = m_registers.BG2Y;
	if (BG3X_dirty)
		m_registers.BG3X_latch = m_registers.BG3X;
	if (BG3Y_dirty)
		m_registers.BG3Y_latch = m_registers.BG3Y;
	BG2X_dirty = false; BG2Y_dirty = false; BG3X_dirty = false; BG3Y_dirty = false;

	//attempt to latch in new enable bits for bg
	latchBackgroundEnableBits();

	m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, schedTimestamp+1007);
}

void PPU::VBlank()
{
	uint64_t schedTimestamp = m_scheduler->getEventTime();
	m_lineCycles = 0;
	m_state=PPUState::VBlank;

	if (!vblank_setHblankBit)	//handle first part of line, up until hblank
	{
		setHBlankFlag(true);

		if (((m_registers.DISPSTAT >> 4) & 0b1))
			m_scheduler->addEvent(Event::HBlankIRQ, &PPU::onHBlankIRQEvent, (void*)this, schedTimestamp + 4);

		vblank_setHblankBit = true;
		m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, schedTimestamp+225);
		if (m_registers.VCOUNT < 162)
			DMAVideoCaptureCallback(callbackContext);
		return;
	}

	vblank_setHblankBit = false;	//end of vblank for current line
	setHBlankFlag(false);
	m_registers.VCOUNT++;
	if (m_registers.VCOUNT == 228)		//go back to drawing
	{
		//copy over new values to affine regs
		m_registers.BG2X_latch = m_registers.BG2X;
		m_registers.BG2Y_latch = m_registers.BG2Y;
		m_registers.BG3X_latch = m_registers.BG3X;
		m_registers.BG3Y_latch = m_registers.BG3Y;
		BG2X_dirty = false; BG2Y_dirty = false; BG3X_dirty = false; BG3Y_dirty = false;

		affineVerticalMosaicCounter = 0;

		setVBlankFlag(false);
		inVBlank = false;
		m_registers.VCOUNT = 0;
		checkVCOUNTInterrupt();
		m_state = PPUState::HDraw;

		renderThread = std::thread(&PPU::drawScreen, this);
	}
	else
	{
		if (m_registers.VCOUNT == 227)			//set vblank flag false on last line of vblank (if applicable)
			setVBlankFlag(false);
		checkVCOUNTInterrupt();
	}

	//attempt to latch in new enable bits for bg. i think this is instant in vblank as there being a 3 scanline delay would make no sense..
	for (int i = 0; i < 4; i++)
	{
		m_backgroundLayers[i].scanlinesSinceEnable = 0;
		if (m_backgroundLayers[i].pendingEnable)
		{
			m_backgroundLayers[i].pendingEnable = false;
			m_backgroundLayers[i].enabled = true;
		}
	}
	m_scheduler->addEvent(Event::PPU, &PPU::onSchedulerEvent, (void*)this, schedTimestamp + 1007);
}

void PPU::checkVCOUNTInterrupt()
{
	uint16_t vcountCompare = ((m_registers.DISPSTAT >> 8) & 0xFF);
	bool vcountMatches = (vcountCompare == m_registers.VCOUNT);
	setVCounterFlag(vcountMatches);
	if (vcountMatches && ((m_registers.DISPSTAT >> 5) & 0b1) && !vcountIRQLine)
		m_interruptManager->requestInterrupt(InterruptType::VCount);
	vcountIRQLine = vcountMatches;
}

void PPU::drawScreen()
{
	//this is extremely hacky right now.
	//TODO: capture PPU IO registers into buffer, use some sync primitive to wait until IO registers for current scanline are ready
	//Need to remove dumb VCOUNT hack too :P (absolutely evil hack)
	//Maybe, instead of force-joining each vblank, have some more complex loop that will block the thread until vblank ends (creating threads is expensive!!)
	for (int i = 0; i < 160; i++)
	{
		//this is dumb
		int lastVCOUNT = m_registers.VCOUNT;
		m_registers.VCOUNT = i;
		uint8_t mode = m_registers.DISPCNT & 0b111;
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
		for (int i = 0; i < 4; i++)
			m_backgroundLayers[i].masterEnable = false;
		affineHorizontalMosaicCounter = 0;
		m_registers.VCOUNT = lastVCOUNT;
	}
}

void PPU::renderMode0()
{
	drawSprites();
	for (int i = 0; i < 4; i++)	//todo: optimise. can use a single for loop and get each pixel one by one
	{
		m_backgroundLayers[i].masterEnable = true;
		if (m_backgroundLayers[i].enabled)
			drawBackground(i);
	}

}

void PPU::renderMode1()
{
	drawSprites();
	for (int i = 0; i < 3; i++)	//todo: optimise. can use a single for loop and get each pixel one by one
	{
		if (m_backgroundLayers[i].enabled)
		{
			m_backgroundLayers[i].masterEnable = true;
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
		m_backgroundLayers[i].masterEnable = true;
		if (m_backgroundLayers[i].enabled)
			drawRotationScalingBackground(i);
	}
}

void PPU::renderMode3()
{
	m_backgroundLayers[2].masterEnable = true;

	drawSprites(true);
	bool mosaic = ((m_registers.BG2CNT >> 6) & 0b1);
	if ((m_registers.DISPCNT >> 10) & 0b1)
	{
		int32_t xRef = m_registers.BG2X_latch & 0xFFFFFFF;
		if ((xRef >> 27) & 0b1)
			xRef |= 0xF0000000;
		int32_t yRef = m_registers.BG2Y_latch & 0xFFFFFFF;
		if ((yRef >> 27) & 0b1)
			yRef |= 0xF000000;

		int16_t pA = (int16_t)m_registers.BG2PA;
		int16_t pC = (int16_t)m_registers.BG2PC;

		m_backgroundLayers[2].priorityBits = m_registers.BG2CNT & 0b11;
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
}

void PPU::renderMode4()
{
	m_backgroundLayers[2].masterEnable = true;

	drawSprites(true);
	uint32_t base = 0;
	bool pageFlip = ((m_registers.DISPCNT >> 4) & 0b1);
	if (pageFlip)
		base = 0xA000;
	bool mosaic = ((m_registers.BG2CNT >> 6) & 0b1);
	if ((m_registers.DISPCNT >> 10) & 0b1)
	{
		int32_t xRef = m_registers.BG2X_latch & 0xFFFFFFF;
		if ((xRef >> 27) & 0b1)
			xRef |= 0xF0000000;
		int32_t yRef = m_registers.BG2Y_latch & 0xFFFFFFF;
		if ((yRef >> 27) & 0b1)
			yRef |= 0xF000000;

		int16_t pA = (int16_t)m_registers.BG2PA;
		int16_t pC = (int16_t)m_registers.BG2PC;
		m_backgroundLayers[2].priorityBits = m_registers.BG2CNT & 0b11;
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
}

void PPU::renderMode5()
{
	m_backgroundLayers[2].masterEnable = true;

	drawSprites(true);
	uint32_t baseAddr = 0;
	bool pageFlip = ((m_registers.DISPCNT >> 4) & 0b1);
	if (pageFlip)
		baseAddr = 0xA000;
	bool mosaic = ((m_registers.BG2CNT >> 6) & 0b1);
	if ((m_registers.DISPCNT >> 10) & 0b1)
	{
		int32_t xRef = m_registers.BG2X_latch & 0xFFFFFFF;
		if ((xRef >> 27) & 0b1)
			xRef |= 0xF0000000;
		int32_t yRef = m_registers.BG2Y_latch & 0xFFFFFFF;
		if ((yRef >> 27) & 0b1)
			yRef |= 0xF000000;

		int16_t pA = (int16_t)m_registers.BG2PA;
		int16_t pC = (int16_t)m_registers.BG2PC;

		m_backgroundLayers[2].priorityBits = m_registers.BG2CNT & 0b11;
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
}

void PPU::composeLayers()
{
	uint16_t backDrop = *(uint16_t*)m_mem->paletteRAM & 0x7FFF;
	int spriteMosaicHorizontal = ((m_registers.MOSAIC >> 8) & 0xF) + 1;
	int spriteHorizontalMosaicCounter = 0;
	int spriteMosaicX = 0;
	bool mosaicInProcess = false;
	for (int x = 0; x < 240; x++)
	{	
		if (((m_registers.DISPCNT >> 7) & 0b1))				//forced blank -> white screen
		{
			m_renderBuffer[pageIdx][(240 * m_registers.VCOUNT) + x] = 0xFFFFFFFF;
			continue;
		}

		Window pointInfo = getWindowAttributes(x, m_registers.VCOUNT);

		uint16_t finalCol = backDrop;
		bool transparentSpriteTop = false;
		uint16_t blendPixelB = 0x8000;
		if (((m_registers.BLDCNT >> 13) & 0b1))
			blendPixelB = backDrop;
		uint16_t blendPixelA = 0x8000;
		if (((m_registers.BLDCNT >> 5) & 0b1))
			blendPixelA = backDrop;

		int highestBlendBPrio = 255, blendALayer = 255;

		int highestPriority = 255;
		for (int layer = 3; layer >= 0; layer--)
		{
			if (m_backgroundLayers[layer].enabled && m_backgroundLayers[layer].masterEnable && pointInfo.layerDrawable[layer])	//layer activated
			{
				uint16_t colAtLayer = m_backgroundLayers[layer].lineBuffer[x];
				if (!((colAtLayer >> 15) & 0b1))
				{
					bool isBlendTargetA = ((m_registers.BLDCNT >> layer) & 0b1);
					if ((m_backgroundLayers[layer].priorityBits <= highestPriority))
					{
						highestPriority = m_backgroundLayers[layer].priorityBits;
						finalCol = colAtLayer;
						blendPixelA = 0x8000;					//blend target A must be top visible layer, so if this isn't a blend target it gets disabled
						blendALayer = 255;
						if ((m_registers.BLDCNT >> layer) & 0b1)
						{
							blendPixelA = finalCol;
							blendALayer = layer;
						}

					}

					//target b is next highest priority layer that isn't target a
					if ((m_backgroundLayers[layer].priorityBits <= highestBlendBPrio) && (blendALayer!=layer))
					{
						blendPixelB = 0x8000;
						if ((m_registers.BLDCNT >> (layer + 8)) & 0b1)
						{
							highestBlendBPrio = m_backgroundLayers[layer].priorityBits;
							blendPixelB = colAtLayer;
						}
					}
				}

			}
		}
		
		if (((m_registers.DISPCNT >> 12) & 0b1) && pointInfo.objDrawable)
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
				bool isBlendTargetA = (((m_registers.BLDCNT >> 4) & 0b1) || m_spriteAttrBuffer[spriteX].transparent);
				if (m_spriteAttrBuffer[spriteX].priority <= highestPriority)
				{
					finalCol = spritePixel;
					if (isBlendTargetA)
					{
						//weird!! blend target b might not have been selected if the topmost layer before was A+B, so make sure we set it now
						if (blendALayer <= 3 && ((m_registers.BLDCNT >> (8 + blendALayer)) & 0b1))
							blendPixelB = blendPixelA;

						transparentSpriteTop = m_spriteAttrBuffer[spriteX].transparent;
						blendPixelA = finalCol;
						blendALayer = 255;
					}
					else
						blendPixelA = 0x8000;
				}
				//only choose as target b if not target a (i.e. not transparent), and not topmost visible layer
				if (m_spriteAttrBuffer[x].priority > highestPriority && m_spriteAttrBuffer[x].priority <= highestBlendBPrio)
				{
					blendPixelB = 0x8000;
					if (((m_registers.BLDCNT >> 12) & 0b1))
						blendPixelB = spritePixel;
				}
			}
		}

		//not sure about this. pokemon ruby depends on semi-transparent sprites bypassing the blend enable check. todo: confirm on hardware!
		if (pointInfo.blendable || transparentSpriteTop)
		{
			uint8_t blendMode = ((m_registers.BLDCNT >> 6) & 0b11);
			if (transparentSpriteTop)	
			{
				if (!(blendPixelB >> 15))									//i think blend mode only gets overridden if there's a second target pixel?
					blendMode = 1;
				else if (!((m_registers.BLDCNT >> 4) & 0b1) && (blendPixelB >> 15))		//hmm, if it's only 'semi-transparent' but there's no target B (and sprite not target A), then don't blend??
					blendMode = 0;
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

		m_renderBuffer[pageIdx][(240 * m_registers.VCOUNT) + x] = col16to32(finalCol);

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
	int mosaicHorizontal = (m_registers.MOSAIC & 0xF) + 1;
	int mosaicVertical = ((m_registers.MOSAIC >> 4) & 0xF) + 1;
	switch (bg)
	{
	case 0:
		ctrlReg = m_registers.BG0CNT;
		xScroll = m_registers.BG0HOFS;
		yScroll = m_registers.BG0VOFS;
		break;
	case 1:
		ctrlReg = m_registers.BG1CNT;
		xScroll = m_registers.BG1HOFS;
		yScroll = m_registers.BG1VOFS;
		break;
	case 2:
		ctrlReg = m_registers.BG2CNT;
		xScroll = m_registers.BG2HOFS;
		yScroll = m_registers.BG2VOFS;
		break;
	case 3:
		ctrlReg = m_registers.BG3CNT;
		xScroll = m_registers.BG3HOFS;
		yScroll = m_registers.BG3VOFS;
		break;
	}

	bool mosaicEnabled = ((ctrlReg >> 6) & 0b1);

	uint8_t bgPriority = ctrlReg & 0b11;
	uint32_t tileDataBaseBlock = ((ctrlReg >> 2) & 0b11);
	bool hiColor = ((ctrlReg >> 7) & 0b1);
	uint32_t bgMapBaseBlock = ((ctrlReg >> 8) & 0x1F);
	int screenSize = ((ctrlReg >> 14) & 0b11);
	static constexpr int xSizeLut[4] = { 255,511,255,511 };
	static constexpr int ySizeLut[4] = { 255,255,511,511 };
	int xWrap = xSizeLut[screenSize];
	int yWrap = ySizeLut[screenSize];

	int y = m_registers.VCOUNT;
	if (mosaicEnabled)
		y = (y / mosaicVertical) * mosaicVertical;

	uint32_t fetcherY = ((y + yScroll) & yWrap);
	if (screenSize && (fetcherY > 255))					//messy, fix!
	{
		fetcherY -= 256;
		bgMapBaseBlock += 1;	
		if (screenSize == 3)	//not completely sure
			bgMapBaseBlock += 1;
	}

	uint32_t bgMapYIdx = ((fetcherY / 8) * 32) * 2; //each row is 32 chars - each char is 2 bytes

	m_backgroundLayers[bg].priorityBits = bgPriority;

	int screenX = 0;
	int tileFetchIdx = xScroll;
	while (screenX < 240)
	{
		int baseBlockOffset = 0;
		int normalizedTileFetchIdx = tileFetchIdx&xWrap;
		if (screenSize && normalizedTileFetchIdx>255)		//not sure how/why this works anymore, but i'll roll with it /shrug
		{
			normalizedTileFetchIdx -= 256;
			baseBlockOffset++;
		}
		uint32_t bgMapBaseAddress = ((bgMapBaseBlock + baseBlockOffset) * 2048) + bgMapYIdx;
		bgMapBaseAddress += ((normalizedTileFetchIdx>>3) * 2);

		uint8_t tileLower = m_mem->VRAM[bgMapBaseAddress];
		uint8_t tileHigher = m_mem->VRAM[bgMapBaseAddress + 1];
		uint16_t tile = ((uint16_t)tileHigher << 8) | tileLower;

		uint32_t tileNumber = tile & 0x3FF;
		uint32_t paletteNumber = ((tile >> 12) & 0xF);
		bool verticalFlip = ((tile >> 11) & 0b1);
		bool horizontalFlip = ((tile >> 10) & 0b1);

		int yMod8 = ((fetcherY & 7));
		if (verticalFlip)
			yMod8 = 7 - yMod8;

		static constexpr uint32_t tileByteSizeLUT[2] = { 32,64 };
		static constexpr uint32_t tileRowPitchLUT[2] = { 4,8 };

		uint32_t tileMapBaseAddress = (tileDataBaseBlock * 16384) + (tileNumber * tileByteSizeLUT[hiColor]); //find correct tile based on just the tile number
		tileMapBaseAddress += (yMod8 * tileRowPitchLUT[hiColor]);									//then extract correct row of tile info, row pitch says how large each row is in bytes

		//todo: clean this bit up
		int initialTileIdx = ((tileFetchIdx >> 3) & 0xFF);
		int renderTileIdx = initialTileIdx;
		while(initialTileIdx==renderTileIdx)	//keep rendering pixels from this tile until next tile reached (i.e. we're done)
		{
			int pixelOffset = tileFetchIdx & 7;
			if (horizontalFlip)
				pixelOffset = 7 - pixelOffset;
			uint16_t col = 0x8000;
			if (hiColor)
			{
				int paletteEntry = m_mem->VRAM[tileMapBaseAddress + pixelOffset];
				uint32_t paletteMemoryAddr = paletteEntry << 1;

				if (paletteEntry)
				{
					uint8_t colLow = m_mem->paletteRAM[paletteMemoryAddr];
					uint8_t colHigh = m_mem->paletteRAM[paletteMemoryAddr + 1];
					col = ((colHigh << 8) | colLow) & 0x7FFF;
				}
			}
			else
			{
				uint8_t tileData = m_mem->VRAM[tileMapBaseAddress + (pixelOffset >> 1)];
				int stepTile = ((pixelOffset & 0b1)) << 2;
				int colorId = ((tileData >> stepTile) & 0xf);
				if (colorId)
				{
					uint32_t paletteMemoryAddr = paletteNumber * 32;
					paletteMemoryAddr += (colorId * 2);
					uint8_t colLow = m_mem->paletteRAM[paletteMemoryAddr];
					uint8_t colHigh = m_mem->paletteRAM[paletteMemoryAddr + 1];
					col = ((colHigh << 8) | colLow) & 0x7FFF;
				}
			}

			if (screenX < 240)
				m_backgroundLayers[bg].lineBuffer[screenX] = col;
			screenX++;

			if(!mosaicEnabled)
				tileFetchIdx++;
			else
				tileFetchIdx = ((screenX / mosaicHorizontal) * mosaicHorizontal) + xScroll;		//if mosaic enabled, align to mosaic grid instead of just moving to next pixel

			renderTileIdx = (tileFetchIdx >> 3) & 0xFF;
		}
	}
}

void PPU::drawRotationScalingBackground(int bg)
{
	uint16_t ctrlReg = 0;
	int32_t xRef = 0, yRef = 0;
	int16_t pA = 0, pB = 0, pC = 0, pD = 0;
	bool isTarget1 = ((m_registers.BLDCNT >> bg) & 0b1);
	bool isTarget2 = ((m_registers.BLDCNT >> (bg + 8)) & 0b1);
	switch (bg)
	{
	case 2:
		ctrlReg = m_registers.BG2CNT;
		xRef = m_registers.BG2X_latch&0xFFFFFFF;
		yRef = m_registers.BG2Y_latch&0xFFFFFFF;
		pA = m_registers.BG2PA; pB = m_registers.BG2PB; pC = m_registers.BG2PC; pD = m_registers.BG2PD;
		if ((m_registers.BG2X_latch >> 27) & 0b1)
			xRef |= 0xF0000000;
		if ((m_registers.BG2Y_latch >> 27) & 0b1)
			yRef |= 0xF0000000;
		break;
	case 3:
		ctrlReg = m_registers.BG3CNT;
		xRef = m_registers.BG3X_latch&0xFFFFFFF;
		yRef = m_registers.BG3Y_latch&0xFFFFFFF;
		pA = m_registers.BG3PA; pB = m_registers.BG3PB; pC = m_registers.BG3PC; pD = m_registers.BG3PD;
		if ((m_registers.BG3X_latch >> 27) & 0b1)
			xRef |= 0xF0000000;
		if ((m_registers.BG3Y_latch >> 27) & 0b1)
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

	m_backgroundLayers[bg].priorityBits = bgPriority;

	bool mosaicEnabled = (ctrlReg >> 6) & 0b1;

	for (int x = 0; x < 240; x++, m_calcAffineCoords(mosaicEnabled,xRef,yRef,pA,pC))
	{
		uint32_t plotAddr = (m_registers.VCOUNT * 240) + x;
		uint32_t fetcherY = (uint32_t)((int32_t)yRef >> 8);
		if ((fetcherY >= yWrap) && !overflowWrap)
		{
			m_backgroundLayers[bg].lineBuffer[x] = 0x8000;
			continue;
		}
		fetcherY = fetcherY & (yWrap-1);

		uint32_t xCoord = (uint32_t)((int32_t)xRef >> 8);
		if ((xCoord >= xWrap) && !overflowWrap)
		{
			m_backgroundLayers[bg].lineBuffer[x] = 0x8000;
			continue;
		}
		xCoord = xCoord & (xWrap-1);

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
		tileMapBaseAddress += ((fetcherY & 7) * 8);
		uint16_t col = extractColorFromTile(tileMapBaseAddress, xCoord&7, true, false, 0);

		m_backgroundLayers[bg].lineBuffer[x] = col;
	}

}

void PPU::drawSprites(bool bitmapMode)
{
	memset(m_spriteAttrBuffer, 0b00011111, 240);
	memset(m_spriteLineBuffer, 0x80, 480);
	m_spriteCyclesElapsed = 0;
	if (!((m_registers.DISPCNT >> 12) & 0b1))
		return;
	bool oneDimensionalMapping = ((m_registers.DISPCNT >> 6) & 0b1);
	bool isBlendTarget1 = ((m_registers.BLDCNT >> 4) & 0b1);
	bool isBlendTarget2 = ((m_registers.BLDCNT >> 12) & 0b1);
	uint8_t blendMode = ((m_registers.BLDCNT >> 6) & 0b11);

	int mosaicHorizontal = ((m_registers.MOSAIC >> 8) & 0xF) + 1;
	int mosaicVertical = ((m_registers.MOSAIC >> 12) & 0xF) + 1;

	bool limitSpriteCycles = ((m_registers.DISPCNT >> 5) & 0b1);
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

		int renderY = m_registers.VCOUNT;
		if (curSpriteEntry->mosaic)
			renderY = (renderY / mosaicVertical) * mosaicVertical;

		int spriteTop = curSpriteEntry->yCoord;
		if (spriteTop >= 160)							//bit of a dumb hack to accommodate for when sprites are offscreen
			spriteTop -= 256;
		int spriteLeft = curSpriteEntry->xCoord;
		if (spriteLeft >= 240)
			spriteLeft -= 512;
		if (spriteTop > renderY)	//nope. sprite is offscreen or too low
			continue;
		int spriteBottom = 0, spriteRight = 0;
		int rowPitch = 1;	//find out how many lines we have to 'cross' to get to next row (in 1d mapping)
		//need to find out dimensions first to figure out whether to ignore this object
		int spritePriority = curSpriteEntry->priority;

		int spriteBoundsLookupId = (curSpriteEntry->shape << 2) | curSpriteEntry->size;
		static constexpr int spriteXBoundsLUT[12] ={8,16,32,64,16,32,32,64,8,8,16,32};
		static constexpr int spriteYBoundsLUT[12] ={8,16,32,64,8,8,16,32,16,32,32,64};
		static constexpr int xPitchLUT[12] ={1,2,4,8,2,4,4,8,1,1,2,4};

		spriteRight = spriteLeft + spriteXBoundsLUT[spriteBoundsLookupId];
		spriteBottom = spriteTop + spriteYBoundsLUT[spriteBoundsLookupId];
		rowPitch = xPitchLUT[spriteBoundsLookupId];	
		if (m_registers.VCOUNT >= spriteBottom)	//nope, we're past it.
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
	bool oneDimensionalMapping = ((m_registers.DISPCNT >> 6) & 0b1);
	bool isBlendTarget1 = ((m_registers.BLDCNT >> 4) & 0b1);
	bool isBlendTarget2 = ((m_registers.BLDCNT >> 12) & 0b1);
	uint8_t blendMode = ((m_registers.BLDCNT >> 6) & 0b11);

	bool isObjWindow = (curSpriteEntry->objMode == 2);
	bool doubleSize = curSpriteEntry->disableObj;	//odd: bit 9 is 'double-size' flag with affine sprites

	int spriteTop = curSpriteEntry->yCoord;
	if (spriteTop >= 160)
		spriteTop -= 256;
	int spriteLeft = curSpriteEntry->xCoord;
	if (spriteLeft >= 240)
		spriteLeft -= 512;

	if (spriteTop > m_registers.VCOUNT)
		return;

	int spriteBottom = 0, spriteRight = 0;
	int rowPitch = 1;	//find out how many lines we have to 'cross' to get to next row (in 1d mapping)

	//need to find out dimensions first to figure out whether to ignore this object
	int spriteBoundsLookupId = (curSpriteEntry->shape << 2) | curSpriteEntry->size;
	static constexpr int spriteXBoundsLUT[12] = { 8,16,32,64,16,32,32,64,8,8,16,32 };
	static constexpr int spriteYBoundsLUT[12] = { 8,16,32,64,8,8,16,32,16,32,32,64 };
	static constexpr int xPitchLUT[12] = { 1,2,4,8,2,4,4,8,1,1,2,4 };


	spriteRight = spriteLeft + spriteXBoundsLUT[spriteBoundsLookupId];
	spriteBottom = spriteTop + spriteYBoundsLUT[spriteBoundsLookupId];
	rowPitch = xPitchLUT[spriteBoundsLookupId];
	int doubleSizeOffset = ((spriteBottom - spriteTop)) * doubleSize;
	if (m_registers.VCOUNT >= (spriteBottom+doubleSizeOffset))	//nope, we're past it.
		return;

	uint32_t tileId = curSpriteEntry->charName;
	bool hiColor = curSpriteEntry->bitDepth;
	if (hiColor)
		rowPitch *= 2;
	int yOffsetIntoSprite = m_registers.VCOUNT - spriteTop;
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
		int xOffsIntoTile = px & 7;
		uint32_t tileMapLookupAddr = objBaseAddress + yCorrection + (curXSpanTile * ((hiColor) ? 64 : 32));

		int plotCoord = x + spriteLeft;
		if (plotCoord > 239 || plotCoord < 0)
			continue;

		uint16_t col = extractColorFromTile(tileMapLookupAddr, xOffsIntoTile, hiColor, true, curSpriteEntry->paletteNumber);

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

	uint8_t bldCoefficient = min(16, m_registers.BLDY & 0x1F);

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
	uint8_t bldCoeffA = min(16, m_registers.BLDALPHA & 0x1F);
	redA = (redA * bldCoeffA) / 16;
	greenA = (greenA * bldCoeffA) / 16;
	blueA = (blueA * bldCoeffA) / 16;

	uint8_t redB = (colB & 0x1F);
	uint8_t greenB = (colB >> 5) & 0x1F;
	uint8_t blueB = (colB >> 10) & 0x1F;
	uint8_t bldCoeffB = min(16,((m_registers.BLDALPHA >> 8) & 0x1F));
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

Window PPU::getWindowAttributes(int x, int y)
{
	Window activeWindow = {};
	bool window0Enabled = ((m_registers.DISPCNT >> 13) & 0b1);
	bool window1Enabled = ((m_registers.DISPCNT >> 14) & 0b1);
	bool objWindowEnabled = ((m_registers.DISPCNT >> 15) & 0b1) && ((m_registers.DISPCNT >> 12) & 0b1);
	if (!(window0Enabled || window1Enabled || objWindowEnabled))		//drawable if neither window enabled
	{
		activeWindow.blendable = true;
		activeWindow.objDrawable = true;
		activeWindow.layerDrawable[0] = true; activeWindow.layerDrawable[1] = true;
		activeWindow.layerDrawable[2] = true; activeWindow.layerDrawable[3] = true;
		return activeWindow;
	}
	
	//todo: handling garbage window y coords isn't completely correct. should fix !!
	for (int i = 0; i < 2; i++)
	{
		if (((m_registers.DISPCNT >> (13 + i)) & 0b1) && (x >= m_windows[i].x1 && (x < m_windows[i].x2 || m_windows[i].x1 > m_windows[i].x2)) && (y >= m_windows[i].y1 && (y < m_windows[i].y2 || m_windows[i].y1 > m_windows[i].y2)))
			return m_windows[i];
	}
	if (objWindowEnabled && m_spriteAttrBuffer[x].objWindow)
		return m_windows[2];

	//if point not in any window 
	return m_windows[3];
}

void PPU::latchBackgroundEnableBits()
{
	for (int i = 0; i < 4; i++)
	{
		if (m_backgroundLayers[i].pendingEnable)
		{
			m_backgroundLayers[i].scanlinesSinceEnable++;

			//seems to be a 3 scanline delay from the enable bit being set, and the background subsequently being actually enabled. i wonder why..
			if (m_backgroundLayers[i].scanlinesSinceEnable == 3 || m_backgroundLayers[i].enabled)
			{
				m_backgroundLayers[i].scanlinesSinceEnable = 0;
				m_backgroundLayers[i].pendingEnable = false;
				m_backgroundLayers[i].enabled = true;
			}
		}
	}
}

void PPU::setVBlankFlag(bool value)
{
	m_registers.DISPSTAT &= ~0b1;
	m_registers.DISPSTAT |= value;
}

void PPU::setHBlankFlag(bool value)
{
	m_registers.DISPSTAT &= ~0b10;
	m_registers.DISPSTAT |= (value << 1);
}

void PPU::setVCounterFlag(bool value)
{
	m_registers.DISPSTAT &= ~0b100;
	m_registers.DISPSTAT |= (value << 2);
}

uint8_t PPU::readIO(uint32_t address)
{
	switch (address)
	{
	case 0x04000000:
		return m_registers.DISPCNT & 0xFF;
	case 0x04000001:
		return ((m_registers.DISPCNT >> 8) & 0xFF);
	case 0x04000004:
		return m_registers.DISPSTAT & 0xFF;
	case 0x04000005:
		return ((m_registers.DISPSTAT >> 8) & 0xFF);
	case 0x04000006:
		return m_registers.VCOUNT & 0xFF;
	case 0x04000007:
		return ((m_registers.VCOUNT >> 8) & 0xFF);
	case 0x04000008:
		return m_registers.BG0CNT & 0xFF;
	case 0x04000009:
		return ((m_registers.BG0CNT >> 8) & 0xDF);
	case 0x0400000A:
		return m_registers.BG1CNT & 0xFF;
	case 0x0400000B:
		return ((m_registers.BG1CNT >> 8) & 0xDF);
	case 0x0400000C:
		return m_registers.BG2CNT & 0xFF;
	case 0x0400000D:
		return ((m_registers.BG2CNT >> 8) & 0xFF);
	case 0x0400000E:
		return m_registers.BG3CNT & 0xFF;
	case 0x0400000F:
		return ((m_registers.BG3CNT >> 8) & 0xFF);
	case 0x04000048:
		return m_registers.WININ & 0x3F;
	case 0x04000049:
		return ((m_registers.WININ >> 8) & 0x3F);
	case 0x0400004A:
		return m_registers.WINOUT & 0x3F;
	case 0x0400004B:
		return ((m_registers.WINOUT >> 8) & 0x3F);
	case 0x04000050:
		return m_registers.BLDCNT & 0xFF;
	case 0x04000051:
		return ((m_registers.BLDCNT >> 8) & 0x3F);
	case 0x04000052:
		return m_registers.BLDALPHA & 0x1F;
	case 0x04000053:
		return ((m_registers.BLDALPHA >> 8) & 0x1F);
	}

	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unknown PPU IO register read {:#x}", address));
	return 0;
}

void PPU::writeIO(uint32_t address, uint8_t value)
{
	switch (address)
	{
	case 0x04000000:
		m_registers.DISPCNT &= 0xFF00; m_registers.DISPCNT |= value;
		break;
	case 0x04000001:
		m_registers.DISPCNT &= 0x00FF; m_registers.DISPCNT |= (value << 8);

		for (int i = 8; i < 12; i++)
		{
			if (((m_registers.DISPCNT >> i) & 0b1))
			{
				m_backgroundLayers[i - 8].pendingEnable = true;
				if (inVBlank)
				{
					m_backgroundLayers[i - 8].pendingEnable = false;
					m_backgroundLayers[i - 8].enabled = true;
				}
			}
			else
			{
				m_backgroundLayers[i - 8].pendingEnable = false;
				m_backgroundLayers[i - 8].enabled = false;
			}
		}
		break;
	case 0x04000004:
		m_registers.DISPSTAT &= 0b1111111100000111; value &= 0b11111000;
		m_registers.DISPSTAT |= value;
		break;
	case 0x04000005:
		m_registers.DISPSTAT &= 0x00FF; m_registers.DISPSTAT |= (value << 8);
		checkVCOUNTInterrupt();
		break;
	case 0x04000008:
		m_registers.BG0CNT &= 0xFF00; m_registers.BG0CNT |= value;
		break;
	case 0x04000009:
		m_registers.BG0CNT &= 0xFF; m_registers.BG0CNT |= (value << 8);
		break;
	case 0x0400000A:
		m_registers.BG1CNT &= 0xFF00; m_registers.BG1CNT |= value;
		break;
	case 0x0400000B:
		m_registers.BG1CNT &= 0xFF; m_registers.BG1CNT |= (value << 8);
		break;
	case 0x0400000C:
		m_registers.BG2CNT &= 0xFF00; m_registers.BG2CNT |= value;
		break;
	case 0x0400000D:
		m_registers.BG2CNT &= 0xFF; m_registers.BG2CNT |= (value << 8);
		break;
	case 0x0400000E:
		m_registers.BG3CNT &= 0xFF00; m_registers.BG3CNT |= value;
		break;
	case 0x0400000F:
		m_registers.BG3CNT &= 0xFF; m_registers.BG3CNT |= (value << 8);
		break;
	case 0x04000010:
		m_registers.BG0HOFS &= 0xFF00; m_registers.BG0HOFS |= value;
		break;
	case 0x04000011:
		m_registers.BG0HOFS &= 0xFF; m_registers.BG0HOFS |= (((value&0b1) << 8));
		break;
	case 0x04000012:
		m_registers.BG0VOFS &= 0xFF00; m_registers.BG0VOFS |= value;
		break;
	case 0x04000013:
		m_registers.BG0VOFS &= 0xFF; m_registers.BG0VOFS |= (((value&0b1) << 8));
		break;
	case 0x04000014:
		m_registers.BG1HOFS &= 0xFF00; m_registers.BG1HOFS |= value;
		break;
	case 0x04000015:
		m_registers.BG1HOFS &= 0xFF; m_registers.BG1HOFS |= (((value&0b1) << 8));
		break;
	case 0x04000016:
		m_registers.BG1VOFS &= 0xFF00; m_registers.BG1VOFS |= value;
		break;
	case 0x04000017:
		m_registers.BG1VOFS &= 0xFF; m_registers.BG1VOFS |= (((value&0b1) << 8));
		break;
	case 0x04000018:
		m_registers.BG2HOFS &= 0xFF00; m_registers.BG2HOFS |= value;
		break;
	case 0x04000019:
		m_registers.BG2HOFS &= 0xFF; m_registers.BG2HOFS |= (((value&0b1) << 8));
		break;
	case 0x0400001A:
		m_registers.BG2VOFS &= 0xFF00; m_registers.BG2VOFS |= value;
		break;
	case 0x0400001B:
		m_registers.BG2VOFS &= 0xFF; m_registers.BG2VOFS |= (((value&0b1) << 8));
		break;
	case 0x0400001C:
		m_registers.BG3HOFS &= 0xFF00; m_registers.BG3HOFS |= value;
		break;
	case 0x0400001D:
		m_registers.BG3HOFS &= 0xFF; m_registers.BG3HOFS |= (((value&0b1) << 8));
		break;
	case 0x0400001E:
		m_registers.BG3VOFS &= 0xFF00; m_registers.BG3VOFS |= value;
		break;
	case 0x0400001F:
		m_registers.BG3VOFS &= 0xFF; m_registers.BG3VOFS |= (((value&0b1) << 8));
		break;
	case 0x04000040:
		m_windows[0].x2 = min(240,value);
		break;
	case 0x04000041:
		m_windows[0].x1 = value;
		break;
	case 0x04000042:
		m_windows[1].x2 = min(240, value);
		break;
	case 0x04000043:
		m_windows[1].x1 = value;
		break;
	case 0x04000044:
		m_windows[0].y2 = value;
		break;
	case 0x04000045:
		m_windows[0].y1 = value;
		break;
	case 0x04000046:
		m_windows[1].y2 = value;
		break;
	case 0x04000047:
		m_windows[1].y1 = value;
		break;
	case 0x04000048:												//WININ/WINOUT code could be rewritten to avoid this duplication stuff..
		m_registers.WININ &= 0xFF00; m_registers.WININ |= value;
		for (int i = 0; i < 4; i++)
			m_windows[0].layerDrawable[i] = (m_registers.WININ >> i) & 0b1;
		m_windows[0].objDrawable = (m_registers.WININ >> 4) & 0b1;
		m_windows[0].blendable = (m_registers.WININ >> 5) & 0b1;
		break;
	case 0x04000049:
		m_registers.WININ &= 0xFF; m_registers.WININ |= (value << 8);
		for (int i = 0; i < 4; i++)
			m_windows[1].layerDrawable[i] = (m_registers.WININ >> (8 + i)) & 0b1;
		m_windows[1].objDrawable = (m_registers.WININ >> 12) & 0b1;
		m_windows[1].blendable = (m_registers.WININ >> 13) & 0b1;
		break;
	case 0x0400004A:
		m_registers.WINOUT &= 0xFF00; m_registers.WINOUT |= value;
		for (int i = 0; i < 4; i++)
			m_windows[3].layerDrawable[i] = (m_registers.WINOUT >> i) & 0b1;
		m_windows[3].objDrawable = (m_registers.WINOUT >> 4) & 0b1;
		m_windows[3].blendable = (m_registers.WINOUT >> 5) & 0b1;
		break;
	case 0x0400004B:
		m_registers.WINOUT &= 0xFF; m_registers.WINOUT |= (value << 8);
		for (int i = 0; i < 4; i++)
			m_windows[2].layerDrawable[i] = (m_registers.WINOUT >> (8 + i)) & 0b1;
		m_windows[2].objDrawable = (m_registers.WINOUT >> 12) & 0b1;
		m_windows[2].blendable = (m_registers.WINOUT >> 13) & 0b1;
		break;
	case 0x04000020:
		m_registers.BG2PA &= 0xFF00; m_registers.BG2PA |= value;
		break;
	case 0x04000021:
		m_registers.BG2PA &= 0xFF; m_registers.BG2PA |= (value << 8);
		break;
	case 0x04000022:
		m_registers.BG2PB &= 0xFF00; m_registers.BG2PB |= value;
		break;
	case 0x04000023:
		m_registers.BG2PB &= 0xFF; m_registers.BG2PB |= (value << 8);
		break;
	case 0x04000024:
		m_registers.BG2PC &= 0xFF00; m_registers.BG2PC |= value;
		break;
	case 0x04000025:
		m_registers.BG2PC &= 0xFF; m_registers.BG2PC |= (value << 8);
		break;
	case 0x04000026:
		m_registers.BG2PD &= 0xFF00; m_registers.BG2PD |= value;
		break;
	case 0x04000027:
		m_registers.BG2PD &= 0xFF; m_registers.BG2PD |= (value << 8);
		break;
	case 0x04000028:
		m_registers.BG2X &= 0xFFFFFF00; m_registers.BG2X |= value;
		BG2X_dirty = true;
		break;
	case 0x04000029:
		m_registers.BG2X &= 0xFFFF00FF; m_registers.BG2X |= (value << 8);
		BG2X_dirty = true;
		break;
	case 0x0400002A:
		m_registers.BG2X &= 0xFF00FFFF; m_registers.BG2X |= (value << 16);
		BG2X_dirty = true;
		break;
	case 0x0400002B:
		m_registers.BG2X &= 0x00FFFFFF; m_registers.BG2X |= (value << 24);
		BG2X_dirty = true;
		if (inVBlank)
		{
			BG2X_dirty = false;
			m_registers.BG2X_latch = m_registers.BG2X;
		}
		break;
	case 0x0400002C:
		m_registers.BG2Y &= 0xFFFFFF00; m_registers.BG2Y |= value;
		BG2Y_dirty = true;
		break;
	case 0x0400002D:
		m_registers.BG2Y &= 0xFFFF00FF; m_registers.BG2Y |= (value << 8);
		BG2Y_dirty = true;
		break;
	case 0x0400002E:
		m_registers.BG2Y &= 0xFF00FFFF; m_registers.BG2Y |= (value << 16);
		BG2Y_dirty = true;
		break;
	case 0x0400002F:
		m_registers.BG2Y &= 0x00FFFFFF; m_registers.BG2Y |= (value << 24);
		BG2Y_dirty = true;
		if (inVBlank)
		{
			BG2Y_dirty = false;
			m_registers.BG2Y_latch = m_registers.BG2Y;
		}
		break;
	case 0x04000030:
		m_registers.BG3PA &= 0xFF00; m_registers.BG3PA |= value;
		break;
	case 0x04000031:
		m_registers.BG3PA &= 0xFF; m_registers.BG3PA |= (value << 8);
		break;
	case 0x04000032:
		m_registers.BG3PB &= 0xFF00; m_registers.BG3PB |= value;
		break;
	case 0x04000033:
		m_registers.BG3PB &= 0xFF; m_registers.BG3PB |= (value << 8);
		break;
	case 0x04000034:
		m_registers.BG3PC &= 0xFF00; m_registers.BG3PC |= value;
		break;
	case 0x04000035:
		m_registers.BG3PC &= 0xFF; m_registers.BG3PC |= (value << 8);
		break;
	case 0x04000036:
		m_registers.BG3PD &= 0xFF00; m_registers.BG3PD |= value;
		break;
	case 0x04000037:
		m_registers.BG3PD &= 0xFF; m_registers.BG3PD |= (value << 8);
		break;
	case 0x04000038:
		m_registers.BG3X &= 0xFFFFFF00; m_registers.BG3X |= value;
		BG3X_dirty = true;
		break;
	case 0x04000039:
		m_registers.BG3X &= 0xFFFF00FF; m_registers.BG3X |= (value << 8);
		BG3X_dirty = true;
		break;
	case 0x0400003A:
		m_registers.BG3X &= 0xFF00FFFF; m_registers.BG3X |= (value << 16);
		BG3X_dirty = true;
		break;
	case 0x0400003B:
		m_registers.BG3X &= 0x00FFFFFF; m_registers.BG3X |= (value << 24);
		BG3X_dirty = true;
		if (inVBlank)
		{
			BG3X_dirty = false;
			m_registers.BG3X_latch = m_registers.BG3X;
		}
		break;
	case 0x0400003C:
		m_registers.BG3Y &= 0xFFFFFF00; m_registers.BG3Y |= value;
		BG3Y_dirty = true;
		break;
	case 0x0400003D:
		m_registers.BG3Y &= 0xFFFF00FF; m_registers.BG3Y |= (value << 8);
		BG3Y_dirty = true;
		break;
	case 0x0400003E:
		m_registers.BG3Y &= 0xFF00FFFF; m_registers.BG3Y |= (value << 16);
		BG3Y_dirty = true;
		break;
	case 0x0400003F:
		m_registers.BG3Y &= 0x00FFFFFF; m_registers.BG3Y |= (value << 24);
		BG3Y_dirty = true;
		if (inVBlank)
		{
			BG3Y_dirty = false;
			m_registers.BG3Y_latch = m_registers.BG3Y;
		}
		break;
	case 0x04000050:
		m_registers.BLDCNT &= 0xFF00; m_registers.BLDCNT |= value;
		break;
	case 0x04000051:
		m_registers.BLDCNT &= 0xFF; m_registers.BLDCNT |= (value << 8);
		break;
	case 0x04000052:
		m_registers.BLDALPHA &= 0xFF00; m_registers.BLDALPHA |= value;
		break;
	case 0x04000053:
		m_registers.BLDALPHA &= 0xFF; m_registers.BLDALPHA |= (value << 8);
		break;
	case 0x04000054:
		m_registers.BLDY = value;
		break;
	case 0x0400004C:
		m_registers.MOSAIC &= 0xFF00; m_registers.MOSAIC |= value;
		break;
	case 0x0400004D:
		m_registers.MOSAIC &= 0xFF; m_registers.MOSAIC |= (value << 8);
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
	return m_registers.VCOUNT;
}

uint32_t PPU::m_safeDisplayBuffer[240 * 160] = {};