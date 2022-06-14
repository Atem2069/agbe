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
	PPU=0,
	TIMER0=1,
	TIMER1=2,
	TIMER2=3,
	TIMER3=4,
	DMA=5
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
	SchedulerEntry entries[6];	//todo: add moree stuff to scheduler
	const int NUM_ENTRIES = 6;
};