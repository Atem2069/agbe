#pragma once

#include"Logger.h"
#include"Bus.h"
#include"ARM7TDMI.h"
#include"PPU.h"
#include"Input.h"

class GBA
{
public:
	GBA();
	~GBA();

	void run();

	void* getPPUData();
	void registerInput(std::shared_ptr<InputState> inp);
private:
	std::shared_ptr<Bus> m_bus;
	std::shared_ptr<ARM7TDMI> m_cpu;
	std::shared_ptr<PPU> m_ppu;
	std::shared_ptr<Input> m_input;

	std::shared_ptr<InputState> m_inp;

	void m_initialise();
};