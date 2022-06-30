#include"APU.h"

APU::APU(std::shared_ptr<Scheduler> scheduler)
{
	for (int i = 0; i < 2; i++)
		m_channels[i] = {};

	m_scheduler = scheduler;
	m_scheduler->addEvent(Event::AudioSample, &APU::sampleEventCallback, (void*)this, cyclesPerSample);
}

APU::~APU()
{

}

void APU::registerDMACallback(callbackFn dmaCallback, void* context)
{
	FIFODMACallback = dmaCallback;
	dmaContext = dmaCallback;
}

uint8_t APU::readIO(uint32_t address)
{
	switch (address)
	{
	case 0x04000082:
		return (SOUNDCNT_H & 0xFF);
	case 0x04000083:
		return ((SOUNDCNT_H >> 8) & 0xFF);
	case 0x04000084:
		return SOUNDCNT_X;
	case 0x04000088:
		return SOUNDBIAS & 0xFF;
	case 0x04000089:
		return ((SOUNDBIAS >> 8) & 0xFF);
	}

	return 0;
}

void APU::writeIO(uint32_t address, uint8_t value)
{
	switch (address)
	{
	case 0x04000082:
		SOUNDCNT_H &= 0xFF00; SOUNDCNT_H |= value;
		break;
	case 0x04000083:
		SOUNDCNT_H &= 0xFF; SOUNDCNT_H |= (value << 8);
		if ((SOUNDCNT_H >> 11) & 0b1)
			m_channels[0].empty();
		break;
	case 0x04000084:
		SOUNDCNT_X = value;
		break;
	case 0x04000088:
		SOUNDBIAS &= 0xFF00; SOUNDBIAS |= value;
		break;
	case 0x04000089:
		SOUNDBIAS &= 0xFF; SOUNDBIAS |= (value << 8);
		break;
	case 0x040000A0: case 0x040000A1: case 0x040000A2: case 0x040000A3:
		if (!m_channels[0].isFull())
			m_channels[0].push(value);
	//todo: audio fifo writes - and in addition the bits in SOUNDCNT_H that can reset the fifos
	}
}

void APU::onSampleEvent()
{
	//todo
	m_scheduler->addEvent(Event::AudioSample, &APU::sampleEventCallback, (void*)this, m_scheduler->getEventTime() + cyclesPerSample);
}

void APU::onTimer0Overflow()
{
	bool soundEnabled = ((SOUNDCNT_X >> 7) & 0b1);
	if (!soundEnabled)
		return;
	bool channelATimerSelect = ((SOUNDCNT_H >> 10) & 0b1);
	if (!channelATimerSelect)
	{
		if (m_channels[0].isEmpty())
			FIFODMACallback(dmaContext);
		else
			m_channels[0].pop();
	}
}

void APU::onTimer1Overflow()
{
	bool soundEnabled = ((SOUNDCNT_X >> 7) & 0b1);
	if (!soundEnabled)
		return;
	bool channelATimerSelect = ((SOUNDCNT_H >> 10) & 0b1);
	if (channelATimerSelect)
	{
		if (m_channels[0].isEmpty())
			FIFODMACallback(dmaContext);
		else
			m_channels[0].pop();
	}
}

void APU::sampleEventCallback(void* context)
{
	APU* thisPtr = (APU*)context;
	thisPtr->onSampleEvent();
}

void APU::timer0Callback(void* context)
{
	APU* thisPtr = (APU*)context;
	thisPtr->onTimer0Overflow();
}

void APU::timer1Callback(void* context)
{
	APU* thisPtr = (APU*)context;
	thisPtr->onTimer1Overflow();
}