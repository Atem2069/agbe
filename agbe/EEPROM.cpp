#include"EEPROM.h"

EEPROM::EEPROM()
{
	state = WriteState::RequestType;
	readbackCount = 0;
}

EEPROM::~EEPROM()
{

}

uint8_t EEPROM::read(uint32_t address)
{
	uint8_t retData = 1;
	if (!isReading || !activeRead)
		return 1;

	readbackCount++;
	if (readbackCount > 4)
	{
		retData = (readData >> 63) & 0b1;
		readData <<= 1;
	}

	if (readbackCount == 68)
	{
		readbackCount = 0;
		activeRead = false;
	}
	return retData;
}

void EEPROM::write(uint32_t address, uint8_t value)
{
	//this whole code sucks, lol
	value &= 0b1;
	tempWriteBits <<= 1;
	tempWriteBits |= (value & 0b1);
	writeCount++;

	switch (state)
	{
	case WriteState::RequestType:
		if (writeCount == 2)
		{
			if ((tempWriteBits&0b11) == 3)
				isReading = true;
			else
				isReading = false;

			state = WriteState::Address;
			tempWriteBits = 0;
			writeCount = 0;
		}
		break;
	case WriteState::Address:
		readAddress |= ((uint16_t)value << (13- (writeCount-1)));
		if (writeCount == 14)	//todo: account for smaller address
		{
			if (readAddress > 1024)
				readAddress &= 0b1111111111;
			if (isReading)
			{
				readData = ROMData[readAddress];
				activeRead = true;
				readbackCount = 0;
			}
			else
				writeAddress = readAddress;

			state = WriteState::Data;
			tempWriteBits = 0;
			writeCount = 0;
			readAddress = 0;
		}
		break;
	case WriteState::Data:
		if (isReading)
		{
			state = WriteState::RequestType;
			tempWriteBits = 0;
			writeCount = 0;
		}
		else
		{
			if (writeCount <= 64)
				newROMData |=  ((uint64_t)value << (63-(writeCount - 1)));
			if (writeCount == 65)
			{
				ROMData[writeAddress] = newROMData;
				state = WriteState::RequestType;
				tempWriteBits = 0;
				newROMData = 0;
				writeCount = 0;
				writeAddress = 0;
			}
		}
		break;
	}
}