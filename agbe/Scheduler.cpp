#include"Scheduler.h"

Scheduler::Scheduler()
{
	timestamp = 0;
}

Scheduler::~Scheduler()
{

}

void Scheduler::tick(uint64_t cycles)
{

}

uint64_t Scheduler::getCurrentTimestamp()
{
	return timestamp;
}

void Scheduler::addEvent(Event type, callbackFn callback, void* context, uint64_t time)
{

}

void Scheduler::removeEvent(Event type)
{

}

bool Scheduler::getEntryAtTimestamp(SchedulerEntry& entry)
{
	return false;
}