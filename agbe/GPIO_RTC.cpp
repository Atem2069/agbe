#include"GPIO_RTC.h"

RTC::RTC()
{
	m_state = GPIOState::Ready;
	m_dataLatch = 0;
}

RTC::~RTC()
{

}

uint8_t RTC::read(uint32_t address)
{
	//Logger::getInstance()->msg(LoggerSeverity::Info, std::format("GPIO read addr={:#x}", address));
	if (!readWriteMask)
		return 0x0;

	switch (address)
	{
	case 0x080000C4:
		return (data & (~directionMask)) & 0xF;
	case 0x080000C6:
		return directionMask & 0xF;
	case 0x080000C8:
		return readWriteMask & 0b1;
	}
}

//according to gbatek: rom space writes can only be 16/32 bit..hmm
void RTC::write16(uint32_t address, uint16_t value)
{
	//Logger::getInstance()->msg(LoggerSeverity::Info, std::format("GPIO write addr={:#x} val={:#x}", address, value));

	switch (address)
	{
	case 0x080000C4:
		m_writeDataRegister((uint8_t)value);
		break;
	case 0x080000C6:
		directionMask = value & 0xF;
		break;
	case 0x080000C8:
		readWriteMask = value & 0b1;
		break;
	}
}

void RTC::write32(uint32_t address, uint32_t value)
{
	write16(address, (uint16_t)value & 0xFFFF);
	switch (address)										//handle 32 bit reads when they need to be split up into separate register reads (i.e. reading port data/direction regs)
	{
	case 0x080000C4: case 0x080000C6:
		write16(address + 2, (uint16_t)((value>>16)&0xFFFF));
		break;
	}
}

void RTC::m_writeDataRegister(uint8_t value)
{
	//bit 0: SCK, bit 1: SIO, bit 2: CS

	uint8_t oldData = data;
	data = value & 0xF;
	bool csRising = (~((oldData >> 2) & 0b1)) & ((data >> 2) & 0b1);	//CS low before, now high
	bool csFalling = ((oldData >> 2) & 0b1) & (~((data >> 2) & 0b1));	//CS high before, now low

	switch (m_state)
	{
	case GPIOState::Ready:
	{
		if ((data & 0b1) && csRising)
		{
			m_state = GPIOState::Command;
			Logger::getInstance()->msg(LoggerSeverity::Info, "GPIO transfer started. .");
		}
		break;
	}

	case GPIOState::Command:
	{
		//todo
		m_state = GPIOState::Read;	//weird stub :)
		break;
	}

	case GPIOState::Read:
	{
		//todo

		if (csFalling)
		{
			m_state = GPIOState::Ready;
			Logger::getInstance()->msg(LoggerSeverity::Info, "GPIO transfer ended!");
		}
		break;
	}

	case GPIOState::Write:
	{
		//todo

		if (csFalling)
		{
			m_state = GPIOState::Ready;
			Logger::getInstance()->msg(LoggerSeverity::Info, "GPIO transfer ended!");
		}
		break;
	}

	}

}