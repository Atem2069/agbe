#include"EEPROM.h"

EEPROM::EEPROM()
{

}

EEPROM::~EEPROM()
{

}

uint8_t EEPROM::read(uint32_t address)
{
	uint8_t readData = 1;
	if (!isRead)
		return 1;
	if (isRead && readbackProgress < 68 && readbackProgress >= 4)
	{
		int shiftIdx = (63 - (readbackProgress-4));
		readData = (readbackData >> shiftIdx)&0b1;
		std::cout << "read data" << (int)readData << '\n';
	}

	if (readbackProgress == 68)
	{
		readbackProgress = -1;
		isRead = false;
	}

	readbackProgress++;
	return (readData & 0b1);
}

void EEPROM::write(uint32_t address, uint8_t value)
{
	//this whole code sucks, lol
	value &= 0b1;
	if (requestProgress < 2)
		requestData |= (value << requestProgress);
	if (requestProgress == 1)
	{
		if ((requestData & 0b11) == 3)
			isRead = true;
		else
			isRead = false;
		requestData = 0;
	}
	if (requestProgress >= 2 && requestProgress <= 15)
		curAddress |= ((uint16_t)value << (13 - (requestProgress - 2)));
	if ((requestProgress >= 16) && (requestProgress <= 79) && !isRead)
	{
		dataToWrite |= ((uint64_t)value << (63 - (requestProgress - 16)));
		if (requestProgress == 79)
		{
			ROMData[curAddress] = dataToWrite;
			std::cout << "write " << std::hex << (uint32_t)curAddress << " " << std::hex <<  " " << (uint64_t)dataToWrite << '\n';
			curAddress = 0;
			dataToWrite = 0;
		}
	}

	if ((requestProgress == 16) && isRead)
	{
		requestProgress = -1;
		readbackData = ROMData[curAddress];
		std::cout << "read req " << std::hex << (uint32_t)curAddress << '\n';

		curAddress = 0;
	}
	if ((requestProgress == 80))
	{
		requestProgress = -1;
	}

	requestProgress++;
}