#include"APU.h"

APU::APU()
{
	for (int i = 0; i < 2; i++)
		m_channels[i] = {};
}

APU::~APU()
{

}

uint8_t APU::readIO(uint32_t address)
{
	switch (address)
	{
	case 0x04000082:
		return (SOUNDCNT_H & 0xFF);
	case 0x04000083:
		return ((SOUNDCNT_H >> 8) & 0xFF);
	case 0x04000084:
		return SOUNDCNT_X;
	case 0x04000088:
		return SOUNDBIAS & 0xFF;
	case 0x04000089:
		return ((SOUNDBIAS >> 8) & 0xFF);
	}

	return 0;
}

void APU::writeIO(uint32_t address, uint8_t value)
{
	switch (address)
	{
	case 0x04000082:
		SOUNDCNT_H &= 0xFF00; SOUNDCNT_H |= value;
		break;
	case 0x04000083:
		SOUNDCNT_H &= 0xFF; SOUNDCNT_H |= (value << 8);
		break;
	case 0x04000084:
		SOUNDCNT_X = value;
		break;
	case 0x04000088:
		SOUNDBIAS &= 0xFF00; SOUNDBIAS |= value;
		break;
	case 0x04000089:
		SOUNDBIAS &= 0xFF; SOUNDBIAS |= (value << 8);
		break;
	//todo: audio fifo writes - and in addition the bits in SOUNDCNT_H that can reset the fifos
	}
}