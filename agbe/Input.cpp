#include"Input.h"

Input::Input()
{
	
}

Input::~Input()
{

}

void Input::update(InputState newInput)
{
	keyInput |= 0x0FFF;	//bits 14,15 are irq bits

	if (newInput.A)
		keyInput &= ~0b1;
	if (newInput.B)
		keyInput &= ~0b10;
	if (newInput.Select)
		keyInput &= ~0b100;
	if (newInput.Start)
		keyInput &= ~0b1000;
	if (newInput.Right)
		keyInput &= ~0b10000;
	if (newInput.Left)
		keyInput &= ~0b100000;
	if (newInput.Up)
		keyInput &= ~0b1000000;
	if (newInput.Down)
		keyInput &= ~0b10000000;
	if (newInput.R)
		keyInput &= ~0b100000000;
	if (newInput.L)
		keyInput &= ~0b100000000;

}

uint8_t Input::readIORegister(uint32_t address)
{
	if (address == 0x04000130)
		return keyInput & 0xFF;
	if (address == 0x04000131)
		return ((keyInput >> 8) & 0xFF);

	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Invalid/unmapped IO given - {:#x}", address));
}

void Input::writeIORegister(uint32_t address, uint8_t value)
{
	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unimplemented joypad write {:#x}", address));
}