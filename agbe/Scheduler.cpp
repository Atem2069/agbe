#include"Scheduler.h"

Scheduler::Scheduler()
{
	invalidateAll();
}

Scheduler::~Scheduler()
{

}

void Scheduler::addCycles(uint64_t cycles)
{
	timestamp += cycles;
	pendingCycles += cycles;

	if (shouldSync && (timestamp >= syncDelta))
	{
		shouldSync = false;
		tick();
	}
}

void Scheduler::forceSync(uint64_t delta)
{
	syncDelta = timestamp+delta;
	shouldSync = true;
}

void Scheduler::tick()
{
	SchedulerEntry entry = {};
	pendingCycles=0;
	while (getEntryAtTimestamp(entry))
	{
		//getEntryAtTimestamp will already disable the entry, so it won't fire until re-scheduled!
		if (entry.callback && entry.context)
		{
			eventTime = entry.timestamp;
			entry.callback(entry.context);	//dereferencing nullptr? you sure vs??
		}
		pendingCycles = 0;
	}
}

void Scheduler::jumpToNextEvent()
{
	uint64_t lowestTimestamp = 0xFFFFFFFFFFFFFFFF;
	int lowestEntryIdx = 0;
	for (int i = 0; i < NUM_ENTRIES; i++)
	{
		if ((entries[i].timestamp < lowestTimestamp) && entries[i].enabled)
		{
			lowestEntryIdx = i;
			lowestTimestamp = entries[i].timestamp;
		}
	}

	timestamp = entries[lowestEntryIdx].timestamp;
	eventTime = timestamp;
	m_lastFiredEvent = (Event)lowestEntryIdx;
	entries[lowestEntryIdx].enabled = false;
	entries[lowestEntryIdx].callback(entries[lowestEntryIdx].context);
}

uint64_t Scheduler::getCurrentTimestamp()
{
	return timestamp;
}

uint64_t Scheduler::getEventTime()
{
	return eventTime;
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
			m_lastFiredEvent = (Event)i;
			entry = curEntry;
			return true;
		}
	}

	return false;
}

void Scheduler::invalidateAll()
{
	timestamp = 0;
	eventTime = 0;
	for (int i = 0; i < NUM_ENTRIES; i++)
		entries[i].enabled = false;
}

Event Scheduler::getLastFiredEvent()
{
	return m_lastFiredEvent;
}