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
		setByteInHalfword(&(m_dmaChannels[0].control), value, 1);
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
		setByteInHalfword(&(m_dmaChannels[1].control), value, 1);
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
		setByteInHalfword(&(m_dmaChannels[2].control), value, 1);
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
		setByteInHalfword(&(m_dmaChannels[3].control), value, 1);
		break;

	//default:
	//	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unknown DMA Write: {#x}", address));
	}

	checkDMAChannels();
}

void Bus::checkDMAChannels()
{
	for (int i = 0; i < 4; i++)
	{
		uint16_t curCtrlReg = m_dmaChannels[i].control;
		if ((curCtrlReg >> 15) & 0b1)
		{
			//dma enabled
			uint8_t startTiming = ((curCtrlReg >> 12) & 0b11);
			if ((startTiming == 0) || ((startTiming==1) && m_ppu->getVBlank()) || ((startTiming==2) && m_ppu->getHBlank()))
			{
				doDMATransfer(i);
			}
		}
	}
	//clear hblank/vblank flags now that we've checked all dma channels
	m_ppu->getHBlank(true);
	m_ppu->getVBlank(true);
}

void Bus::doDMATransfer(int channel)
{
	//we assume the transfer is going to take place by the time this function is called
	DMAChannel curChannel = m_dmaChannels[channel];

	uint32_t srcAddrMask = 0x0FFFFFFF;
	if (channel == 0)
		srcAddrMask = 0x07FFFFFF;
	uint32_t destAddrMask = 0x07FFFFFF;
	if (channel == 3)
		destAddrMask = 0x0FFFFFFF;

	uint32_t src = curChannel.srcAddress & srcAddrMask;
	uint32_t dest = curChannel.destAddress & destAddrMask;

	int numWords = curChannel.wordCount;
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
	for (int i = 0; i < numWords; i++)
	{
		if (wordTransfer)
		{
			uint32_t word = read32(src, false);
			write32(dest, word, false);
		}
		else
		{
			uint16_t halfword = read16(src, false);
			write16(dest, halfword, false);
		}

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
		case 2:
			dest -= incrementAmount;
			break;
		}

	}

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
	if(!repeatDMA)
		m_dmaChannels[channel].control &= 0x7FFF;	//clear DMA enable
	dmaInProgress = false;
}