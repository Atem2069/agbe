#pragma once

#include<iostream>

typedef void(*callbackFn)(void*);

struct SchedulerEntry
{
	callbackFn callback;
	void* context;
	uint64_t timestamp;
	bool enabled;	
};

enum class Event
{
	PPU,
	TIMER0,
	TIMER1,
	TIMER2,
	TIMER3,
	DMA
};

class Scheduler
{
public:
	Scheduler();
	~Scheduler();

	void tick(uint64_t cycles);
	uint64_t getCurrentTimestamp();
	void addEvent(Event type, callbackFn callback, void* context, uint64_t time);
	void removeEvent(Event type);
private:
	bool getEntryAtTimestamp(SchedulerEntry& entry);
	uint64_t timestamp;
	SchedulerEntry entries[5];	//todo: add moree stuff to scheduler
};