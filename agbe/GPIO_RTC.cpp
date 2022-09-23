#include"GPIO_RTC.h"

RTC::RTC()
{

}

RTC::~RTC()
{

}

uint8_t RTC::read(uint32_t address)
{
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

}

void RTC::write32(uint32_t address, uint32_t value)
{
	write16(address, (uint16_t)value & 0xFFFF);
	switch (address)										//handle 32 bit reads when they need to be split up into separate register reads (i.e. reading port data/direction regs)
	{
	case 0x080000C4: case 0x080000C6:
		write16(address + 2, (uint16_t)value & 0xFFFF);
		break;
	}
}