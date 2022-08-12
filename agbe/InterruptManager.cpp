#include"InterruptManager.h"

InterruptManager::InterruptManager(std::shared_ptr<Scheduler> scheduler)
{
	m_scheduler = scheduler;
	IE = 0; IF = 0;
}

InterruptManager::~InterruptManager()
{

}

void InterruptManager::requestInterrupt(InterruptType type)
{
	IF |= (1 << (int)type);
	checkIRQs();
}

bool InterruptManager::getInterrupt()
{
	return irqAvailable;
}

uint8_t InterruptManager::readIO(uint32_t address)
{
	switch (address)
	{
	case 0x04000208:
		return IME & 0xFF;
	case 0x04000209:
		return 0;
	case 0x04000200:
		return IE & 0xFF;
	case 0x04000201:
		return ((IE >> 8) & 0xFF);
	case 0x04000202:
		return IF & 0xFF;
	case 0x04000203:
		return ((IF >> 8) & 0xFF);
	}

	return 0;
}

void InterruptManager::writeIO(uint32_t address, uint8_t value)
{
	switch (address)
	{
	case 0x04000208:
		IME = value & 0b1; break;
	case 0x04000200:
		IE &= 0xFF00; IE |= value; break;
	case 0x04000201:
		IE &= 0x00FF; IE |= ((value << 8)); break;
	case 0x04000202:				//why does this work? 
		IF &= ~(value);
		break;
	case 0x04000203:
		IF &= ~((value<<8));
		break;
	}

	checkIRQs();
}

void InterruptManager::onEvent()
{
	pendingIrq = false;
	irqAvailable = true;
}

void InterruptManager::eventHandler(void* context)
{
	InterruptManager* thisPtr = (InterruptManager*)context;
	thisPtr->onEvent();
}

void InterruptManager::checkIRQs()
{
	pendingIrq = ((IF & IE & 0b0011111111111111));
	if (pendingIrq)																													//new irq? schedule irq signal change
	{
		m_scheduler->addEvent(Event::IRQ, &InterruptManager::eventHandler, (void*)this, m_scheduler->getCurrentTimestamp() + 4);
		m_scheduler->forceSync(4);
	}
	else
		irqAvailable = false;																										//otherwise reset irq line
}