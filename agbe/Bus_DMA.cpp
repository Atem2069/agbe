#include"Bus.h"

uint8_t Bus::DMARegRead(uint32_t address)
{
	//meh:
	switch (address)
	{
	case 0x040000BA:
		return m_dmaChannels[0].control & 0xE0;
	case 0x040000BB:
		return (m_dmaChannels[0].control >> 8) & 0xF7;
	case 0x040000C6:
		return (m_dmaChannels[1].control & 0xE0);
	case 0x040000C7:
		return (m_dmaChannels[1].control >> 8) & 0xF7;
	case 0x040000D2:
		return (m_dmaChannels[2].control) & 0xE0;
	case 0x040000D3:
		return (m_dmaChannels[2].control >> 8) & 0xF7;
	case 0x040000DE:
		return (m_dmaChannels[3].control) & 0xE0;
	case 0x040000DF:
		return (m_dmaChannels[3].control >> 8) & 0xFF;
	}
	//Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unknown DMA Read: {:#x}", address));
	return 0;
}

void Bus::DMARegWrite(uint32_t address, uint8_t value)
{
	//oh boy (what am i doing??!)
	switch (address)
	{
	case 0x040000B0:
		setByteInWord(&(m_dmaChannels[0].srcAddress), value, 0);
		break;
	case 0x040000B1:
		setByteInWord(&(m_dmaChannels[0].srcAddress), value, 1);
		break;
	case 0x040000B2:
		setByteInWord(&(m_dmaChannels[0].srcAddress), value, 2);
		break;
	case 0x040000B3:
		setByteInWord(&(m_dmaChannels[0].srcAddress), value, 3);
		break;
	case 0x040000B4:
		setByteInWord(&(m_dmaChannels[0].destAddress), value, 0);
		break;
	case 0x040000B5:
		setByteInWord(&(m_dmaChannels[0].destAddress), value, 1);
		break;
	case 0x040000B6:
		setByteInWord(&(m_dmaChannels[0].destAddress), value, 2);
		break;
	case 0x040000B7:
		setByteInWord(&(m_dmaChannels[0].destAddress), value, 3);
		break;
	case 0x040000B8:
		setByteInHalfword(&(m_dmaChannels[0].wordCount), value, 0);
		break;
	case 0x040000B9:
		setByteInHalfword(&(m_dmaChannels[0].wordCount), value, 1);
		break;
	case 0x040000BA:
		setByteInHalfword(&(m_dmaChannels[0].control), value, 0);
		break;
	case 0x040000BB:
		if ((!((m_dmaChannels[0].control >> 15) & 0b1)) && ((value >> 7) & 0b1))
		{
			m_dmaChannels[0].internalDest = m_dmaChannels[0].destAddress;
			m_dmaChannels[0].internalSrc = m_dmaChannels[0].srcAddress;
		}
		setByteInHalfword(&(m_dmaChannels[0].control), value, 1);
		checkDMAChannel(0);
		break;

	//DMA 1

	case 0x040000BC:
		setByteInWord(&(m_dmaChannels[1].srcAddress), value, 0);
		break;
	case 0x040000BD:
		setByteInWord(&(m_dmaChannels[1].srcAddress), value, 1);
		break;
	case 0x040000BE:
		setByteInWord(&(m_dmaChannels[1].srcAddress), value, 2);
		break;
	case 0x040000BF:
		setByteInWord(&(m_dmaChannels[1].srcAddress), value, 3);
		break;
	case 0x040000C0:
		setByteInWord(&(m_dmaChannels[1].destAddress), value, 0);
		break;
	case 0x040000C1:
		setByteInWord(&(m_dmaChannels[1].destAddress), value, 1);
		break;
	case 0x040000C2:
		setByteInWord(&(m_dmaChannels[1].destAddress), value, 2);
		break;
	case 0x040000C3:
		setByteInWord(&(m_dmaChannels[1].destAddress), value, 3);
		break;
	case 0x040000C4:
		setByteInHalfword(&(m_dmaChannels[1].wordCount), value, 0);
		break;
	case 0x040000C5:
		setByteInHalfword(&(m_dmaChannels[1].wordCount), value, 1);
		break;
	case 0x040000C6:
		setByteInHalfword(&(m_dmaChannels[1].control), value, 0);
		break;
	case 0x040000C7:
		if ((!((m_dmaChannels[1].control >> 15) & 0b1)) && ((value >> 7) & 0b1))
		{
			m_dmaChannels[1].internalDest = m_dmaChannels[1].destAddress;
			m_dmaChannels[1].internalSrc = m_dmaChannels[1].srcAddress;
		}
		setByteInHalfword(&(m_dmaChannels[1].control), value, 1);
		checkDMAChannel(1);
		break;


	//DMA 2

	case 0x040000C8:
		setByteInWord(&(m_dmaChannels[2].srcAddress), value, 0);
		break;
	case 0x040000C9:
		setByteInWord(&(m_dmaChannels[2].srcAddress), value, 1);
		break;
	case 0x040000CA:
		setByteInWord(&(m_dmaChannels[2].srcAddress), value, 2);
		break;
	case 0x040000CB:
		setByteInWord(&(m_dmaChannels[2].srcAddress), value, 3);
		break;
	case 0x040000CC:
		setByteInWord(&(m_dmaChannels[2].destAddress), value, 0);
		break;
	case 0x040000CD:
		setByteInWord(&(m_dmaChannels[2].destAddress), value, 1);
		break;
	case 0x040000CE:
		setByteInWord(&(m_dmaChannels[2].destAddress), value, 2);
		break;
	case 0x040000CF:
		setByteInWord(&(m_dmaChannels[2].destAddress), value, 3);
		break;
	case 0x040000D0:
		setByteInHalfword(&(m_dmaChannels[2].wordCount), value, 0);
		break;
	case 0x040000D1:
		setByteInHalfword(&(m_dmaChannels[2].wordCount), value, 1);
		break;
	case 0x040000D2:
		setByteInHalfword(&(m_dmaChannels[2].control), value, 0);
		break;
	case 0x040000D3:
		if ((!((m_dmaChannels[2].control >> 15) & 0b1)) && ((value >> 7) & 0b1))
		{
			m_dmaChannels[2].internalDest = m_dmaChannels[2].destAddress;
			m_dmaChannels[2].internalSrc = m_dmaChannels[2].srcAddress;
		}
		setByteInHalfword(&(m_dmaChannels[2].control), value, 1);
		checkDMAChannel(2);
		break;


	//DMA 3

	case 0x040000D4:
		setByteInWord(&(m_dmaChannels[3].srcAddress), value, 0);
		break;
	case 0x040000D5:
		setByteInWord(&(m_dmaChannels[3].srcAddress), value, 1);
		break;
	case 0x040000D6:
		setByteInWord(&(m_dmaChannels[3].srcAddress), value, 2);
		break;
	case 0x040000D7:
		setByteInWord(&(m_dmaChannels[3].srcAddress), value, 3);
		break;
	case 0x040000D8:
		setByteInWord(&(m_dmaChannels[3].destAddress), value, 0);
		break;
	case 0x040000D9:
		setByteInWord(&(m_dmaChannels[3].destAddress), value, 1);
		break;
	case 0x040000DA:
		setByteInWord(&(m_dmaChannels[3].destAddress), value, 2);
		break;
	case 0x040000DB:
		setByteInWord(&(m_dmaChannels[3].destAddress), value, 3);
		break;
	case 0x040000DC:
		setByteInHalfword(&(m_dmaChannels[3].wordCount), value, 0);
		break;
	case 0x040000DD:
		setByteInHalfword(&(m_dmaChannels[3].wordCount), value, 1);
		break;
	case 0x040000DE:
		setByteInHalfword(&(m_dmaChannels[3].control), value, 0);
		break;
	case 0x040000DF:
		if ((!((m_dmaChannels[3].control >> 15) & 0b1)) && ((value >> 7) & 0b1))
		{
			m_dmaChannels[3].internalDest = m_dmaChannels[3].destAddress;
			m_dmaChannels[3].internalSrc = m_dmaChannels[3].srcAddress;
		}
		setByteInHalfword(&(m_dmaChannels[3].control), value, 1);
		checkDMAChannel(3);
		break;

	//default:
	//	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unknown DMA Write: {#x}", address));
	}

}

void Bus::checkDMAChannel(int idx)
{
	uint16_t curCtrlReg = m_dmaChannels[idx].control;
	if ((curCtrlReg >> 15) & 0b1)
	{
		//dma enabled
		uint8_t startTiming = ((curCtrlReg >> 12) & 0b11);
		if (startTiming == 0)
		{
			m_scheduler->removeEvent(Event::DMA);
			m_scheduler->addEvent(Event::DMA, &Bus::DMA_ImmediateCallback, (void*)this, m_scheduler->getCurrentTimestamp() + 3);
			m_scheduler->forceSync(3);	//lol (is 3 cycles even accurate?)
		}
	}
}

void Bus::doDMATransfer(int channel)
{
	if (busLocked)
	{
		m_dmaChannels[channel].busLockWait = true;
		m_scheduler->removeEvent(Event::DMA);
		m_scheduler->addEvent(Event::DMA, &Bus::DMA_ImmediateCallback, (void*)this, m_scheduler->getCurrentTimestamp() + 1);
		m_scheduler->forceSync(1);	//lol (is 3 cycles even accurate?)
		return;
	}
	m_openBusVals.dmaJustFinished = false;

	//handle dma priority
	int lastDMAPriority = runningDMAPriority;    //save last dma priority then restore when dma completes. initial val is 255 which is way lower priority than any dma :)
	if (channel > lastDMAPriority)				//if a lower priority dma is running, then oops. wait until it's done
	{
		m_dmaChannels[channel].stalledLowerPriority = true;
		return;
	}
	runningDMAPriority = channel;

	bool dmaWasInProgress = dmaInProgress;
	if(!dmaWasInProgress)
		m_scheduler->addCycles(1);	//2 cycle startup delay?
	//we assume the transfer is going to take place by the time this function is called
	DMAChannel curChannel = m_dmaChannels[channel];

	uint32_t srcAddrMask = 0x0FFFFFFF;
	if (channel == 0)
		srcAddrMask = 0x07FFFFFF;
	uint32_t destAddrMask = 0x07FFFFFF;
	if (channel == 3)
		destAddrMask = 0x0FFFFFFF;

	uint32_t src = curChannel.internalSrc & srcAddrMask;
	uint32_t dest = curChannel.internalDest & destAddrMask;

	int numWords = curChannel.wordCount;
	if (channel != 3)
		numWords &= 0x3FFF;
	if (numWords == 0)
	{
		numWords = 0x4000;
		if (channel == 3)
			numWords = 0x10000;	//channel 3 has higher word count
	}

	if (channel == 3 && !backupInitialised && (numWords==9 || numWords==17))
	{
		//TODO: check address. this isn't completely accurate and might have false positives
		backupInitialised = true;
		m_backupType = BackupType::EEPROM4K;
		if (numWords == 17)
		{
			Logger::getInstance()->msg(LoggerSeverity::Info, "Auto-detected 64K EEPROM chip access!!");
			m_backupType = BackupType::EEPROM64K;
		}
		else
			Logger::getInstance()->msg(LoggerSeverity::Info, "Auto-detected 4K EEPROM chip access!!");
		m_backupMemory = std::make_shared<EEPROM>(m_backupType);
	}

	uint8_t srcAddrCtrl = ((curChannel.control >> 7) & 0b11);
	if ((src >= 0x08000000 && src <= 0x0DFFFFFF))				//dma from rom always uses incrementing addresses regardless of increment setting
		srcAddrCtrl = 0;
	uint8_t dstAddrCtrl = ((curChannel.control >> 5) & 0b11);
	bool wordTransfer = ((curChannel.control >> 10) & 0b1);
	dmaInProgress = true;
	bool reloadDest = false;
	dmaNonsequentialAccess = true;

	bool isAudioDMA = false;
	int dmaType = ((curChannel.control >> 12) & 0b11);
	if (dmaType == 3 && channel != 3)							//need to clean this up: audio fifo dma
	{
		isAudioDMA = true;
		if(dest != 0x040000A0 && dest!=0x040000A4)
			std::cout << "bad write addr: " << std::hex << dest << '\n';
		wordTransfer = true;
		numWords = 4;	//4 32 bit words transferred
		dstAddrCtrl = 2;
		curChannel.control |= 0x200;
	}

	for (int i = 0; i < numWords; i++)		
	{
		m_scheduler->addCycles(2);
		if (wordTransfer)
		{
			uint32_t word = 0;
			if (src <= 0x01FFFFFF)	[[unlikely]]							//handle dma open bus, when r/w to 0->01ffffff
				word = m_openBusVals.dma[channel];
			else
			{
				word = read32(src&~0b11, (AccessType)!dmaNonsequentialAccess);
				m_openBusVals.dma[channel] = word;
			}
			write32(dest&~0b11, word,(AccessType)!dmaNonsequentialAccess);									
		}
		else
		{
			uint16_t halfword = 0;
			if (src <= 0x01FFFFFF)
				halfword = std::rotr(m_openBusVals.dma[channel],8*(dest&0b11));
			else
			{
				halfword = read16(src&~0b1, (AccessType)!dmaNonsequentialAccess);
				m_openBusVals.dma[channel] = (halfword << 16) | halfword;
			}
			write16(dest&~0b1, halfword,(AccessType)!dmaNonsequentialAccess);                        			
		}
		m_scheduler->tick();
		int incrementAmount = (wordTransfer) ? 4 : 2;

		switch (srcAddrCtrl)
		{
		case 0:
			src += incrementAmount;
			if ((src == 0x08000000) && ((dest >= 0x08000000 && dest <= 0x0dffffff)))	//wtf? rom to rom dmas are completely broken on hardware
				src = dest + 2;															//if src crosses into rom, then everything is screwed, and the src starts reading from the dest
			break;																		//(because the first src access is sequential for some reason, so the dest address is used instead)
		case 1:
			src -= incrementAmount;
			break;
		}

		switch (dstAddrCtrl)
		{
		case 0:
			dest += incrementAmount;
			break;
		case 1:
			dest -= incrementAmount;
			break;
		case 3:
			dest += incrementAmount;
			reloadDest = true;
			break;
		}

	}
	m_dmaChannels[channel].internalSrc = src;
	m_dmaChannels[channel].internalDest = dest;
	m_dmaChannels[channel].wordCount = 0;

	if (((curChannel.control >> 14) & 0b1))
	{
		constexpr InterruptType interruptLUT[4] = { InterruptType::DMA0,InterruptType::DMA1,InterruptType::DMA2,InterruptType::DMA3 };
		m_interruptManager->requestInterrupt(interruptLUT[channel]);
	}

	bool repeatDMA = ((curChannel.control >> 9) & 0b1);
	if (repeatDMA && dmaType!=0)
	{
		if (reloadDest)
			m_dmaChannels[channel].internalDest = m_dmaChannels[channel].destAddress;
		m_dmaChannels[channel].internalSrc = src;
		m_dmaChannels[channel].wordCount = numWords;
	}
	else
		m_dmaChannels[channel].control &= 0x7FFF;	//clear DMA enable

	if (!dmaWasInProgress)
	{
		dmaInProgress = false;
		m_scheduler->addCycles(1);
		m_openBusVals.dmaJustFinished = true;
		m_openBusVals.lastDmaVal = m_openBusVals.dma[channel];
	}
	prefetcherHalted = false;

	runningDMAPriority = lastDMAPriority;
	//TODO: could probs handle this a little better, e.g. a check to see if *any* dma is stalled so this doesn't always run
	for (int i = 0; i < 4; i++)
	{
		if (m_dmaChannels[i].stalledLowerPriority)
		{
			m_dmaChannels[i].stalledLowerPriority = false;
			doDMATransfer(i);
		}
	}

}

void Bus::onVBlank()
{
	for (int i = 0; i < 4; i++)
	{
		uint16_t curCtrlReg = m_dmaChannels[i].control;
		if ((curCtrlReg >> 15) & 0b1)
		{
			uint8_t transferTiming = ((curCtrlReg >> 12) & 0b11);
			if (transferTiming == 1)
				doDMATransfer(i);
		}
	}
}

void Bus::onHBlank()
{
	for (int i = 0; i < 4; i++)
	{
		uint16_t curCtrlReg = m_dmaChannels[i].control;
		if ((curCtrlReg >> 15) & 0b1)
		{
			uint8_t transferTiming = ((curCtrlReg >> 12) & 0b11);
			if (transferTiming == 2)
				doDMATransfer(i);
		}
	}
}

void Bus::onImmediate()
{
	for (int i = 0; i < 4; i++)
	{
		uint16_t curCtrlReg = m_dmaChannels[i].control;
		if (((curCtrlReg >> 15) & 0b1))
		{
			//dma enabled
			uint8_t startTiming = ((curCtrlReg >> 12) & 0b11);
			if(startTiming==0 || m_dmaChannels[i].busLockWait)
				doDMATransfer(i);
			m_dmaChannels[i].busLockWait = false;
		}
	}
}

void Bus::onVideoCapture()	//special video capture dma used by dma3 (scanlines 2-162)
{
	uint16_t curCtrlReg = m_dmaChannels[3].control;
	if ((curCtrlReg >> 15) & 0b1)
	{
		uint8_t startTiming = ((curCtrlReg >> 12) & 0b11);
		if (startTiming == 3)
		{
			if (m_ppu->getVCOUNT() == 161)						//clear repeat bit if on the last scanline of dma
				m_dmaChannels[3].control &= 0x7FFF;
			doDMATransfer(3);
		}
	}
}

void Bus::onAudioFIFO(int channel)
{
	static constexpr uint32_t addrLookup[2] = { 0x040000A0,0x040000A4 };
	for (int i = 1; i < 3; i++)
	{
		uint16_t curCtrlReg = m_dmaChannels[i].control;
		if ((curCtrlReg >> 15) & 0b1)
		{
			//dma enabled
			uint8_t startTiming = ((curCtrlReg >> 12) & 0b11);
			if ((startTiming == 3) && m_dmaChannels[i].destAddress==addrLookup[channel])
				doDMATransfer(i);
		}
	}
}

void Bus::DMA_VBlankCallback(void* context)
{
	Bus* thisPtr = (Bus*)context;
	thisPtr->onVBlank();
}

void Bus::DMA_HBlankCallback(void* context)
{
	Bus* thisPtr = (Bus*)context;
	thisPtr->onHBlank();
}

void Bus::DMA_ImmediateCallback(void* context)
{
	Bus* thisPtr = (Bus*)context;
	thisPtr->onImmediate();
}

void Bus::DMA_VideoCaptureCallback(void* context)
{
	Bus* thisPtr = (Bus*)context;
	thisPtr->onVideoCapture();
}

void Bus::DMA_AudioFIFOCallback(void* context, int channel)
{
	Bus* thisPtr = (Bus*)context;
	thisPtr->onAudioFIFO(channel);	
}