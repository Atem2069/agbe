#include"ARM7TDMI.h"

ARM7TDMI::ARM7TDMI(std::shared_ptr<Bus> bus)
{
	m_bus = bus;
	CPSR = 0x1F;	//system starts in system mode
	for (int i = 0; i < 16; i++)
		R[i] = 0;
	R[13] = 0x03007F00;
	R13_irq = 0x03007FA0;
	R13_svc = 0x03007FE0;
	R[15] = 0x08000000;	//start of cartridge
}

ARM7TDMI::~ARM7TDMI()
{

}

void ARM7TDMI::step()
{
	//todo
}