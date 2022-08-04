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
	while (getEntryAtTimestamp(entry))
	{
		eventTime = entry.timestamp;
		entry.callback(entry.context);	
	}
}

void Scheduler::jumpToNextEvent()
{
	SchedulerEntry lowestEntry = *m_entries.begin();
	m_entries.pop_front();				
	timestamp = lowestEntry.timestamp;
	eventTime = timestamp;
	m_lastFiredEvent = lowestEntry.eventType;
	lowestEntry.callback(lowestEntry.context);
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
	newEntry.eventType = type;
	newEntry.enabled = true;
	
	std::list<SchedulerEntry>::iterator it = m_entries.begin();
	bool inserted = false;
	while (it != m_entries.end() && !inserted)
	{
		SchedulerEntry x = *it;
		if (x.timestamp > newEntry.timestamp)
		{
			inserted = true;
			m_entries.insert(it, newEntry);
		}
		it++;
	}
	if (!inserted)
		m_entries.push_back(newEntry);
}

void Scheduler::removeEvent(Event type)
{
	std::list<SchedulerEntry>::iterator it = m_entries.begin();
	bool removed = false;
	while (it != m_entries.end() && !removed)
	{
		if ((*it).eventType == type)
		{
			removed = true;
			m_entries.erase(it);
		}
		it++;
	}
}

bool Scheduler::getEntryAtTimestamp(SchedulerEntry& entry)
{
	std::list<SchedulerEntry>::iterator it = m_entries.begin();
	if ((*it).timestamp <= timestamp)
	{
		m_lastFiredEvent = (*it).eventType;
		entry = *it;
		m_entries.pop_front();
		return true;
	}
	return false;
}

void Scheduler::invalidateAll()
{
	timestamp = 0;
	eventTime = 0;
	m_entries.clear();
}

Event Scheduler::getLastFiredEvent()
{
	return m_lastFiredEvent;
}