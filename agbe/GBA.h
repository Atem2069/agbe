#pragma once

#include"Logger.h"
#include"Bus.h"
#include"ARM7TDMI.h"
#include"PPU.h"

class GBA
{
public:
	GBA();
	~GBA();

	void run();

	void* getPPUData();
private:
	std::shared_ptr<Bus> m_bus;
	std::shared_ptr<ARM7TDMI> m_cpu;
	std::shared_ptr<PPU> m_ppu;

	void m_initialise();
};