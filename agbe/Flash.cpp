#include"Flash.h"

Flash::Flash(BackupType type)
{
	//TODO: account for backup type affecting size
	m_state = FlashState::Ready;
	bank = 0;

	memset(flashMem, 0xFF, 131072);
}

Flash::~Flash()
{

}

uint8_t Flash::read(uint32_t address)
{
	address &= 0xFFFF;
	if (address == 0 && m_state == FlashState::ChipID)
		return 0x62;
	if (address == 1 && m_state == FlashState::ChipID)
		return 0x13;

	return flashMem[(bank * 65536) + address];
}

void Flash::write(uint32_t address, uint8_t value)
{
	FlashState originalState = m_state;	//dumb hack, haha
	address &= 0xFFFF;
	if (address == 0x5555 && m_state == FlashState::Ready)		//todo: check if correct byte even written to initiate command
	{
		m_state = FlashState::Command1;
	}
	if (address == 0x2AAA && m_state == FlashState::Command1)
	{
		m_state = FlashState::Command2;
	}
	if (address == 0x5555)
	{
		if (m_state == FlashState::Command2 || m_state==FlashState::ChipID)
		{
			m_state = FlashState::Ready;
			switch (value)
			{
			case 0x90:
				m_state = FlashState::ChipID;
				break;
			case 0x10:
				for (int i = 0; i < (128 * 1024); i++)
					flashMem[i] = 0xFF;
				break;
			case 0xA0:
				m_state = FlashState::PrepareWrite;
				break;
			case 0xB0:
				m_state = FlashState::SwitchBank;
				break;
			}
		}
	}

	if (originalState == FlashState::PrepareWrite )
	{
		flashMem[address + (bank * 65536)] = value;
		m_state = FlashState::Ready;
	}
	if (m_state == FlashState::Command2 && value==0x30)
	{
		address += (bank * 65536);
		uint32_t endAddr = address + 0xFFFF;
		for (int i = address; i <= endAddr; i++)
			flashMem[address] = 0xFF;
		m_state = FlashState::Ready;
	}
	if (m_state == FlashState::SwitchBank && address == 0)
	{
		bank = value & 0b1;
		m_state = FlashState::Ready;
	}
}