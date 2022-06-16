#include"Input.h"

Input::Input()
{
	keyInput = 0xFFFF;
}

Input::~Input()
{

}

void Input::registerInput(std::shared_ptr<InputState> inputState)
{
	m_inputState = inputState;
}

void Input::tick()
{
	uint16_t newInputState = (~(m_inputState->reg)) & 0x3FF;
	keyInput = newInputState;
}

uint8_t Input::readIORegister(uint32_t address)
{
	if (address == 0x04000130)
		return keyInput & 0xFF;
	if (address == 0x04000131)
		return ((keyInput >> 8) & 0xFF);

	//Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Invalid/unmapped IO given - {:#x}", address));
}

void Input::writeIORegister(uint32_t address, uint8_t value)
{
	//Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unimplemented joypad write {:#x}", address));
}
