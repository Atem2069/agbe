#include"Input.h"

Input::Input(std::shared_ptr<Scheduler> scheduler)
{
	m_scheduler = scheduler;
	m_scheduler->addEvent(Event::Input, &Input::onSchedulerEvent, (void*)this, 280896);
	lastEventTime = 280896;
	keyInput = 0xFFFF;
}

Input::~Input()
{

}

void Input::registerInput(std::shared_ptr<InputState> inputState)
{
	m_inputState = inputState;
}

void Input::event()
{
	uint16_t newInputState = (~(m_inputState->reg)) & 0x3FF;
	keyInput = newInputState;

	uint64_t curTime = m_scheduler->getCurrentTimestamp();
	uint64_t diff = curTime - lastEventTime;

	lastEventTime = (curTime + 280896) - diff;
	m_scheduler->addEvent(Event::Input, &Input::onSchedulerEvent, (void*)this, lastEventTime);
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

void Input::reschedule()
{
	m_scheduler->addEvent(Event::Input, &Input::onSchedulerEvent, (void*)this, 280896);
	lastEventTime = 280896;
}

void Input::onSchedulerEvent(void* context)
{
	Input* thisptr = (Input*)context;
	thisptr->event();
}