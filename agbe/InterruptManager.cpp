#include"InterruptManager.h"

InterruptManager::InterruptManager(std::shared_ptr<Scheduler> scheduler)
{
	m_scheduler = scheduler;
	shadowIF = 0;
}

InterruptManager::~InterruptManager()
{

}

void InterruptManager::requestInterrupt(InterruptType type)
{
	switch (type)
	{
	case InterruptType::VBlank:
		IF |= 0b1; break;
	case InterruptType::HBlank:
		IF |= 0b10; break;
	case InterruptType::VCount:
		IF |= 0b100; break;
	case InterruptType::Timer0:
		IF |= 0b1000; break;
	case InterruptType::Timer1:
		IF |= 0b10000; break;
	case InterruptType::Timer2:
		IF |= 0b100000; break;
	case InterruptType::Timer3:
		IF |= 0b1000000; break;
	case InterruptType::Serial:
		IF |= 0b10000000; break;
	case InterruptType::DMA0:
		IF |= 0b100000000; break;
	case InterruptType::DMA1:
		IF |= 0b1000000000; break;
	case InterruptType::DMA2:
		IF |= 0b10000000000; break;
	case InterruptType::DMA3:
		IF |= 0b100000000000; break;
	case InterruptType::Keypad:
		IF |= 0b1000000000000; break;

	}
	if (!irqPending)
	{
		irqPending = true;
		m_scheduler->addEvent(Event::IRQ, &InterruptManager::eventHandler, (void*)this, m_scheduler->getCurrentTimestamp() + 4);
		m_scheduler->forceSync(4);
	}
}

bool InterruptManager::getInterrupt(bool bypassIMECheck)
{
	if (!IME && !bypassIMECheck)
		return false;
	return shadowIF & IE & 0b0011111111111111;	//probs a better way but oh well
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
		if (!irqPending)
		{
			shadowIF &= ~(value);
			IF = shadowIF;
		}
		else
			IF &= ~(value);
		break;
	case 0x04000203:
		if (!irqPending)
		{
			shadowIF &= ~((value << 8));
			IF = shadowIF;
		}
		else
			IF &= ~(value);
		break;
	}
}

void InterruptManager::onEvent()
{
	irqPending = false;
	shadowIF = IF;
}

void InterruptManager::eventHandler(void* context)
{
	InterruptManager* thisPtr = (InterruptManager*)context;
	thisPtr->onEvent();
}