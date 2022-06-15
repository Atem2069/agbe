#include"Scheduler.h"

Scheduler::Scheduler()
{
	invalidateAll();
}

Scheduler::~Scheduler()
{

}

void Scheduler::tick(uint64_t cycles)
{
	timestamp += cycles;
	SchedulerEntry entry = {};
	while (getEntryAtTimestamp(entry))
	{
		//getEntryAtTimestamp will already disable the entry, so it won't fire until re-scheduled!
		if(entry.callback && entry.context)
			entry.callback(entry.context);	//dereferencing nullptr? you sure vs??
	}
}

uint64_t Scheduler::getCurrentTimestamp()
{
	return timestamp;
}

void Scheduler::addEvent(Event type, callbackFn callback, void* context, uint64_t time)
{
	SchedulerEntry newEntry = {};
	newEntry.timestamp = time;
	newEntry.context = context;
	newEntry.callback = callback;
	newEntry.enabled = true;
	entries[(int)type] = newEntry;
}

void Scheduler::removeEvent(Event type)
{
	entries[(int)type].enabled = false;
}

bool Scheduler::getEntryAtTimestamp(SchedulerEntry& entry)
{
	for (int i = 0; i < NUM_ENTRIES; i++)
	{
		SchedulerEntry curEntry = entries[i];
		if (curEntry.enabled && curEntry.timestamp <= timestamp)
		{
			entries[i].enabled = false;
			entry = curEntry;
			return true;
		}
	}

	return false;
}

void Scheduler::invalidateAll()
{
	timestamp = 0;
	for (int i = 0; i < NUM_ENTRIES; i++)
		entries[i].enabled = false;
}