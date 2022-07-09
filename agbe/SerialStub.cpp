#include"SerialStub.h"

SerialStub::SerialStub(std::shared_ptr<Scheduler> scheduler, std::shared_ptr<InterruptManager> interruptManager)
{
	m_scheduler = scheduler;
	m_interruptManager = interruptManager;
}

SerialStub::~SerialStub()
{

}

uint8_t SerialStub::readIO(uint32_t address)
{
	switch (address)
	{
	case 0x04000128:
		return SIOCNT & 0xFF;
	case 0x04000129:
		return ((SIOCNT >> 8) & 0xFF);
	case 0x0400012A:
		return SIODATA8;
	case 0x04000120:
		return SIODATA32 & 0xFF;
	case 0x04000121:
		return ((SIODATA32 >> 8) & 0xFF);
	case 0x04000122:
		return ((SIODATA32 >> 16) & 0xFF);
	case 0x04000123:
		return ((SIODATA32 >> 24) & 0xFF);
	}
}

void SerialStub::writeIO(uint32_t address, uint8_t value)
{
	switch (address)
	{
	case 0x04000128:
		SIOCNT &= 0xFF00; SIOCNT |= value;
		break;
	case 0x04000129:
	{
		SIOCNT &= 0xFF; SIOCNT |= (value << 8);
		calculateNextEvent();
		break;
	}
	case 0x0400012A:
		SIODATA8 = value;
		break;
	case 0x04000120:
		SIODATA32 &= 0xFFFFFF00; SIODATA32 |= value;
		break;
	case 0x04000121:
		SIODATA32 &= 0xFFFF00FF; SIODATA32 |= (value << 8);
		break;
	case 0x04000122:
		SIODATA32 &= 0xFF00FFFF; SIODATA32 |= (value << 16);
		break;
	case 0x04000123:
		SIODATA32 &= 0xFFFFFF; SIODATA32 |= (value << 24);
		break;
	}
}

void SerialStub::serialEvent()
{
	bool sioEnabled = ((SIOCNT >> 7) & 0b1);
	if (!sioEnabled)
		return;
	eventInProgress = false;

	bool wordTransfer = ((SIOCNT >> 12) & 0b1);
	if (wordTransfer)				//dunno about this bit but whatever
		SIODATA32 = 0xFFFFFFFF;
	else
		SIODATA8 = 0xFF;

	bool doIrq = ((SIOCNT >> 14) & 0b1);
	if (doIrq)
		m_interruptManager->requestInterrupt(InterruptType::Serial);
	SIOCNT &= 01101000001111111;
}

void SerialStub::calculateNextEvent()
{
	bool isInternalClock = SIOCNT & 0b1;
	bool transferEnabled = ((SIOCNT >> 7) & 0b1);
	if (!isInternalClock || eventInProgress || !transferEnabled)
		return;
	eventInProgress = true;

	bool wordTransfer = ((SIOCNT >> 12) & 0b1);
	bool shiftClock = ((SIOCNT >> 1) & 0b1);
	uint64_t cycleLUT[2] = { 64,8 };	//0: 64 clocks per transfer. 1: 8 clocks per transfer
	uint64_t transferLength = (wordTransfer) ? 32 : 8;

	uint64_t transferClocks = (transferLength * cycleLUT[shiftClock]) + m_scheduler->getCurrentTimestamp();
	m_scheduler->removeEvent(Event::Serial);	
	m_scheduler->addEvent(Event::Serial, &SerialStub::eventCallback, (void*)this, transferClocks);
}

void SerialStub::eventCallback(void* context)
{
	SerialStub* thisPtr = (SerialStub*)context;
	thisPtr->serialEvent();
}