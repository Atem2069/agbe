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
	bool sckRising = (~(oldData & 0b1)) & (data & 0b1);					//SCK low before, now high

	switch (m_state)
	{
	case GPIOState::Ready:
	{
		if ((data & 0b1) && csRising)
		{
			m_state = GPIOState::Command;
			m_shiftCount = 0;
			m_command = 0;
			m_dataLatch = 0;
			Logger::getInstance()->msg(LoggerSeverity::Info, "GPIO transfer started..");
		}
		break;
	}

	case GPIOState::Command:
	{
		if (sckRising)
		{
			uint8_t serialData = (data >> 1) & 0b1;
			m_command |= (serialData << m_shiftCount);
			m_shiftCount++;

			if (m_shiftCount == 8)
			{
				m_shiftCount = 0;
				if ((m_command & 0xF0) != 0b01100000)	//bits 4-7 should be 0110, otherwise flip
					m_command = m_reverseBits(m_command);
				Logger::getInstance()->msg(LoggerSeverity::Info, std::format("GPIO command received: {:#x}", m_command));
				
				//should decode bits 1-3 here, decide which register being read/written (important to determine byte size for r/w)...

				//bit 0 denotes whether read/write command. 0=write,1=read
				if (m_command & 0b1)
				{
					Logger::getInstance()->msg(LoggerSeverity::Info, "Process GPIO read command..");
					m_state = GPIOState::Read;
				}
				else
				{
					Logger::getInstance()->msg(LoggerSeverity::Info, "Process GPIO write command..");
					m_state = GPIOState::Write;
				}
			}
		}
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

uint8_t RTC::m_reverseBits(uint8_t a)
{
	uint8_t b = 0;
	//slow...
	for (int i = 0; i < 8; i++)
	{
		uint8_t bit = (a >> i) & 0b1;
		b |= (bit << (7 - i));
	}
	return b;
}