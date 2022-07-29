#include"Flash.h"

Flash::Flash(BackupType type)
{
	//TODO: account for backup type affecting size
	m_state = FlashState::Ready;
	bank = 0;

	saveSize = (type == BackupType::FLASH1M) ? (128 * 1024) : (64 * 1024);
	std::vector<uint8_t> saveData;

	if (getSaveData(saveData))
	{
		memcpy(flashMem, (void*)&saveData[0], saveSize);
	}
	else
		memset(flashMem, 0xFF, 131072);

	switch (type)
	{
	case BackupType::FLASH1M:
		m_manufacturerID = 0x62;
		m_deviceID = 0x13;
		break;
	case BackupType::FLASH512K:
		m_manufacturerID = 0x32;
		m_deviceID = 0x1B;
		break;
	}
}

Flash::~Flash()
{
	writeSaveData(flashMem, saveSize);
}

uint8_t Flash::read(uint32_t address)
{
	address &= 0xFFFF;
	if (address == 0 && inChipID)
		return m_manufacturerID;
	if (address == 1 && inChipID)
		return m_deviceID;

	return flashMem[(bank * 65536) + address];
}

void Flash::write(uint32_t address, uint8_t value)
{
	address &= 0xFFFF;
	bool noWrite = false;
	if (address == 0x5555)
	{
		//handle beginning of command
		if (m_state == FlashState::Ready)
		{
			if (value != 0xAA)
				Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Invalid flash command: {:#x}", (int)value));
			else
				m_state = FlashState::CommandInProgress;
		}

		//after E005555=AA,E002AAA=55, an actual operation is sent to the flash chip
		else if (m_state == FlashState::Operation)
		{
			m_state = FlashState::Ready;
			switch (value)
			{
			case 0x90:
				inChipID = true;
				break;
			case 0xF0:
				inChipID = false;
				break;
			case 0x10:			
				memset(flashMem, 0xFF, 65536 * 2);
				m_state = FlashState::Ready;
				break;
			case 0xA0:
				m_state = FlashState::PrepareWrite;
				noWrite = true;
				break;
			case 0xB0:
				m_state = FlashState::BankSwitch;
				break;
			}
		}
	}

	//second byte in command 'prologue'
	if (address == 0x2AAA && m_state == FlashState::CommandInProgress)
	{
		if (value != 0x55)
			Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Invalid byte while writing command prologue: {:#x}", (int)value));
		else
			m_state = FlashState::Operation;
	}

	//handle erases
	if ((address & 0xFFF) == 0 && m_state == FlashState::Operation)
	{
		int baseAddr = (address & 0xF000);
		for (int i = 0; i < 0x1000; i++)
			flashMem[(bank*65536) + (baseAddr + i)] = 0xFF;

		m_state = FlashState::Ready;
	}

	//handle writes (noWrite flag is an odd hack, but stops this from going through if we just sent the command to initiate a write)
	if (m_state == FlashState::PrepareWrite && !noWrite)
	{
		flashMem[(bank * 65536) + address] = value;
		m_state = FlashState::Ready;
	}

	//bank switching
	if (m_state == FlashState::BankSwitch && address == 0)
	{
		bank = value & 0b1;
		m_state = FlashState::Ready;
	}
}