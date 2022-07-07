#include"EEPROM.h"

EEPROM::EEPROM(BackupType type)
{	state = WriteState::RequestType;
	readbackCount = 0;

	switch (type)
	{
	case BackupType::EEPROM4K:
		addressSize = 6; break;
	case BackupType::EEPROM64K:
		addressSize = 14; break;
	}
	saveSize = (type == BackupType::EEPROM4K) ? 128 : 1024;

	std::vector<uint8_t> saveData;
	if (getSaveData(saveData))
	{
		//we can't just straight up memcpy unfortunately, bc endianness
		for (int i = 0; i < saveSize; i++)
		{
			uint64_t currentWord = 0;
			for (int j = 0; j < 8; j++)
			{
				int curIdx = (i * 8) + j;
				uint8_t curByte = saveData[curIdx];
				currentWord |= ((uint64_t)curByte << ((7-j) * 8));
			}
			ROMData[i] = currentWord;
		}
	}
	else
		memset(ROMData, 0xFF, 1024 * 8);
}

EEPROM::~EEPROM()
{
	//first have to prepare save data
	uint8_t* tempSaveData = new uint8_t[saveSize * 8];

	for (int i = 0; i < saveSize; i++)
	{
		uint64_t curVal = ROMData[i];
		for (int j = 0; j < 8; j++)
		{
			uint8_t writeVal = (curVal >> ((7-j) * 8));
			tempSaveData[(i * 8) + j] = writeVal;	//write out each 8 byte value in big endian order
		}
	}

	writeSaveData((void*)tempSaveData, saveSize*8);
	delete[] tempSaveData;
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
		readAddress |= ((uint16_t)value << ((addressSize-1) - (writeCount - 1)));
		if (writeCount == addressSize)	//todo: account for smaller address
		{
			if (isReading)
			{
				if (readAddress > 1023)
					readData = 0xFFFFFFFFFFFFFFFF;	//out of bounds read returns all 1s
				else
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
				if(writeAddress<=1023)
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