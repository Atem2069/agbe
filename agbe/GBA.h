#pragma once

#include"Logger.h"
#include"Bus.h"
#include"ARM7TDMI.h"

class GBA
{
public:
	GBA();
	~GBA();

	void run();
private:
	std::shared_ptr<Bus> m_bus;
	std::shared_ptr<ARM7TDMI> m_cpu;

	void m_initialise();
};