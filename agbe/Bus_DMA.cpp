#include"Bus.h"

uint8_t Bus::DMARegRead(uint32_t address)
{
	//meh:
	switch (address)
	{
	case 0x040000BA:
		return m_dmaChannels[0].control & 0xFF;
	case 0x040000BB:
		return (m_dmaChannels[0].control >> 8) & 0xFF;
	case 0x040000C6:
		return (m_dmaChannels[1].control & 0xFF);
	case 0x040000C7:
		return (m_dmaChannels[1].control >> 8) & 0xFF;
	case 0x040000D2:
		return (m_dmaChannels[2].control) & 0xFF;
	case 0x040000D3:
		return (m_dmaChannels[2].control >> 8) & 0xFF;
	case 0x040000DE:
		return (m_dmaChannels[3].control) & 0xFF;
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
			m_dmaChannels[0].internalDest = m_dmaChannels[0].destAddress;
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
			m_dmaChannels[1].internalDest = m_dmaChannels[1].destAddress;
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
			m_dmaChannels[2].internalDest = m_dmaChannels[2].destAddress;
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
			m_dmaChannels[3].internalDest = m_dmaChannels[3].destAddress;
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
			doDMATransfer(idx);
	}
}

void Bus::doDMATransfer(int channel)
{
	m_scheduler->addCycles(2);	//2 cycle startup delay?
	//we assume the transfer is going to take place by the time this function is called
	DMAChannel curChannel = m_dmaChannels[channel];

	uint32_t srcAddrMask = 0x0FFFFFFF;
	if (channel == 0)
		srcAddrMask = 0x07FFFFFF;
	uint32_t destAddrMask = 0x07FFFFFF;
	if (channel == 3)
		destAddrMask = 0x0FFFFFFF;

	uint32_t src = curChannel.srcAddress & srcAddrMask;
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

	if (channel == 3 && (numWords == 9))
		std::cout << "weird eeprom?" << '\n';
	//std::cout << "DMA src = " << std::hex << src << " dst= " << std::hex << dest << " length= " << std::hex << numWords << '\n';

	uint8_t srcAddrCtrl = ((curChannel.control >> 7) & 0b11);
	uint8_t dstAddrCtrl = ((curChannel.control >> 5) & 0b11);
	bool wordTransfer = ((curChannel.control >> 10) & 0b1);
	dmaInProgress = true;
	bool reloadDest = false;
	bool firstAccess = true;
	for (int i = 0; i < numWords; i++)		//assuming all accesses are sequential, which is probs not right..
	{
		m_scheduler->addCycles(2);
		if (wordTransfer)
		{
			uint32_t word = read32(src,!firstAccess);
			write32(dest, word,true);									//hmm. first rom write is sequential?
		}
		else
		{
			uint16_t halfword = read16(src,!firstAccess);
			write16(dest, halfword,true);                               //same as above^^
		}
		firstAccess = false;
		int incrementAmount = (wordTransfer) ? 4 : 2;

		//TODO: control mode 3 (increment/reload on dest)
		switch (srcAddrCtrl)
		{
		case 0:
			src += incrementAmount;
			break;
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

	m_dmaChannels[channel].srcAddress = src;
	m_dmaChannels[channel].internalDest = dest;
	m_dmaChannels[channel].wordCount = 0;

	if (((curChannel.control >> 14) & 0b1))
	{
		switch (channel)
		{
		case 0:
			m_interruptManager->requestInterrupt(InterruptType::DMA0); break;
		case 1:
			m_interruptManager->requestInterrupt(InterruptType::DMA1); break;
		case 2:
			m_interruptManager->requestInterrupt(InterruptType::DMA2); break;
		case 3:
			m_interruptManager->requestInterrupt(InterruptType::DMA3); break;
		}
	}

	bool repeatDMA = ((curChannel.control >> 9) & 0b1);
	if (repeatDMA)
	{
		if (reloadDest)
			m_dmaChannels[channel].internalDest = m_dmaChannels[channel].destAddress;
		m_dmaChannels[channel].srcAddress = src;
		m_dmaChannels[channel].wordCount = numWords;
	}
	else
		m_dmaChannels[channel].control &= 0x7FFF;	//clear DMA enable

	dmaInProgress = false;
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
		if ((curCtrlReg >> 15) & 0b1)
		{
			//dma enabled
			uint8_t startTiming = ((curCtrlReg >> 12) & 0b11);
			if (startTiming == 0)
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