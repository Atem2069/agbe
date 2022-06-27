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
	DMA=5,
	Frame=6
};

class Scheduler
{
public:
	Scheduler();
	~Scheduler();

	void addCycles(uint64_t cycles);
	void forceSync(uint64_t delta);
	void tick();
	void jumpToNextEvent();
	uint64_t getCurrentTimestamp();
	void addEvent(Event type, callbackFn callback, void* context, uint64_t time);
	void removeEvent(Event type);
	void invalidateAll();
private:
	bool getEntryAtTimestamp(SchedulerEntry& entry);
	uint64_t timestamp;
	uint64_t pendingCycles = 0;
	uint64_t syncDelta = 0;
	bool shouldSync = false;
	SchedulerEntry entries[7];	//todo: add moree stuff to scheduler
	const int NUM_ENTRIES = 7;
};