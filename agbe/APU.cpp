#include"APU.h"

APU::APU(std::shared_ptr<Scheduler> scheduler)
{
	for (int i = 0; i < 2; i++)
		m_channels[i].empty();

	m_scheduler = scheduler;
	m_scheduler->addEvent(Event::AudioSample, &APU::sampleEventCallback, (void*)this, cyclesPerSample);

	SDL_Init(SDL_INIT_AUDIO);
	SDL_AudioSpec desiredSpec = {}, obtainedSpec = {};
	desiredSpec.freq = sampleRate;
	desiredSpec.format = AUDIO_S16;	//might have to change this and make use of some actual resampling
	desiredSpec.channels = 1;		//todo: support multiple channels too
	desiredSpec.silence = 0;
	desiredSpec.samples = sampleBufferSize;	
	m_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, &obtainedSpec, 0);
	SDL_PauseAudioDevice(m_audioDevice, 0);
}

APU::~APU()
{
	SDL_Quit();
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
	case 0x04000060:
		return SOUND1CNT_L;
	case 0x04000062:
		return SOUND1CNT_H & 0xFF;
	case 0x04000063:
		return ((SOUND1CNT_H >> 8) & 0xFF);
	case 0x04000064:
		return (SOUND1CNT_X & 0xFF);
	case 0x04000065:
		return ((SOUND1CNT_X >> 8) & 0xFF);
	case 0x04000068:
		return SOUND2CNT_L & 0xFF;
	case 0x04000069:
		return ((SOUND2CNT_L >> 8) & 0xFF);
	case 0x0400006C:
		return SOUND2CNT_H & 0xFF;
	case 0x0400006D:
		return ((SOUND2CNT_H >> 8) & 0xFF);
	case 0x04000080:
		return (SOUNDCNT_L & 0xFF);
	case 0x04000081:
		return ((SOUNDCNT_L >> 8) & 0xFF);
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
	case 0x04000060:
		SOUND1CNT_L = value;
		m_square1.sweepShift = SOUND1CNT_L & 0b111;
		m_square1.sweepNegate = ((SOUND1CNT_L >> 3) & 0b1);
		m_square1.sweepPeriod = ((SOUND1CNT_L >> 4) & 0b111);
		break;
	case 0x04000062:
		SOUND1CNT_H &= 0xFF00; SOUND1CNT_H |= value;
		break;
	case 0x04000063:
		SOUND1CNT_H &= 0xFF; SOUND1CNT_H |= (value << 8);
		m_square1.lengthCounter = 64 - (SOUND1CNT_H & 0x1F);
		break;
	case 0x04000064:
		SOUND1CNT_X &= 0xFF00; SOUND1CNT_X |= value;
		break;
	case 0x04000065:
		SOUND1CNT_X &= 0xFF; SOUND1CNT_X |= (value << 8);
		if ((value >> 7) & 0b1)
		{
			SOUNDCNT_X |= 0b1;

			if (m_square1.lengthCounter == 0)
				m_square1.lengthCounter = 64;
			m_square1.frequency = (2048 - (SOUND1CNT_X & 0x7FF)) * 16;
			m_square1.enabled = true;
			m_square1.dutyPattern = (SOUND1CNT_H >> 6) & 0b11;
			m_square1.doLength = ((SOUND1CNT_X >> 14) & 0b1);
			m_square1.volume = ((SOUND1CNT_H >> 12) & 0xF);
			m_scheduler->addEvent(Event::Square1, &APU::square1EventCallback, (void*)this, m_scheduler->getCurrentTimestamp() + m_square1.frequency);
		}
		break;
	case 0x04000068:
		SOUND2CNT_L &= 0xFF00; SOUND2CNT_L |= value;
		break;
	case 0x04000069:
		SOUND2CNT_L &= 0xFF; SOUND2CNT_L |= (value << 8);
		m_square2.lengthCounter = 64 - (SOUND2CNT_L & 0x1F);
		break;
	case 0x0400006C:
		SOUND2CNT_H &= 0xFF00; SOUND2CNT_H |= value;
		break;
	case 0x0400006D:
		SOUND2CNT_H &= 0xFF; SOUND2CNT_H |= (value << 8);
		if ((value >> 7) & 0b1)	//reload channel
		{
			SOUNDCNT_X |= 0b10;	//reenable square 2 in soundcnt_x

			if (m_square2.lengthCounter == 0)
				m_square2.lengthCounter = 64;
			m_square2.frequency = (2048 - (SOUND2CNT_H & 0x7FF)) * 16;
			m_square2.enabled = true;
			m_square2.dutyPattern = (SOUND2CNT_L >> 6) & 0b11;
			m_square2.doLength = ((SOUND2CNT_H >> 14) & 0b1);
			m_square2.volume = ((SOUND2CNT_L >> 12) & 0xF);
			m_scheduler->addEvent(Event::Square2, &APU::square2EventCallback, (void*)this, m_scheduler->getCurrentTimestamp() + m_square2.frequency);
		}
		break;
	case 0x04000080:
		SOUNDCNT_L &= 0xFF00; SOUNDCNT_L |= value;
		break;
	case 0x04000081:
		SOUNDCNT_L &= 0xFF; SOUNDCNT_L |= (value << 8);
		break;
	case 0x04000082:
		SOUNDCNT_H &= 0xFF00; SOUNDCNT_H |= value;
		break;
	case 0x04000083:
		SOUNDCNT_H &= 0xFF; SOUNDCNT_H |= (value << 8);
		if ((SOUNDCNT_H >> 11) & 0b1)
			m_channels[0].empty();
		if ((SOUNDCNT_H >> 15) & 0b1)
			m_channels[1].empty();
		break;
	case 0x04000084:
		SOUNDCNT_X = value & 0x80;	//rest of the bits are read only
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
		break;
	case 0x040000A4: case 0x040000A5: case 0x040000A6: case 0x040000A7:
		if (!m_channels[1].isFull())
			m_channels[1].push(value);
		break;
	}
}

void APU::onSampleEvent()
{
	int16_t chanASample = m_channels[0].currentSample*2;
	int16_t chanBSample = m_channels[1].currentSample*2;

	bool square1OutputEnabled = (((SOUNDCNT_L >> 8) & 0b1)) || (((SOUNDCNT_L >> 12)) & 0b1);
	int8_t square1Sample = m_square1.output >> (2 - (SOUNDCNT_H & 0b11));

	bool square2OutputEnabled = (((SOUNDCNT_L >> 9) & 0b1)) || (((SOUNDCNT_L >> 13)) & 0b1);
	int8_t square2Sample = m_square2.output >> (2 - (SOUNDCNT_H & 0b11));
	int16_t finalSample = (chanASample + chanBSample + square1Sample + square2Sample)*4;
	m_sampleBuffer[sampleIndex] = finalSample;

	sampleIndex++;
	if (sampleIndex == sampleBufferSize)
	{
		sampleIndex = 0;
		int16_t m_finalSamples[sampleBufferSize] = {};
		memcpy((void*)m_finalSamples, (void*)m_sampleBuffer, sampleBufferSize * 2);
		SDL_QueueAudio(m_audioDevice, (void*)m_finalSamples, sampleBufferSize*2);

		while (SDL_GetQueuedAudioSize(m_audioDevice) > sampleBufferSize * 2)
			(void)0;
	}

	m_scheduler->addEvent(Event::AudioSample, &APU::sampleEventCallback, (void*)this, m_scheduler->getEventTime() + cyclesPerSample);
}

void APU::onTimer0Overflow()
{
	bool soundEnabled = ((SOUNDCNT_X >> 7) & 0b1);
	if (!soundEnabled)
		return;

	bool channelATimerSelect = ((SOUNDCNT_H >> 10) & 0b1);
	bool channelBTimerSelect = ((SOUNDCNT_H >> 14) & 0b1);
	if (!channelATimerSelect)
		updateDMAChannel(0);
	if (!channelBTimerSelect)
		updateDMAChannel(1);

	if (m_shouldDMA)
		FIFODMACallback(dmaContext);
	m_shouldDMA = false;
}

void APU::onTimer1Overflow()
{
	bool soundEnabled = ((SOUNDCNT_X >> 7) & 0b1);
	if (!soundEnabled)
		return;

	bool channelATimerSelect = ((SOUNDCNT_H >> 10) & 0b1);
	bool channelBTimerSelect = ((SOUNDCNT_H >> 14) & 0b1);
	if (channelATimerSelect)
		updateDMAChannel(0);
	if (channelBTimerSelect)
		updateDMAChannel(1);

	if (m_shouldDMA)
		FIFODMACallback(dmaContext);
	m_shouldDMA = false;
}

void APU::onSquare1FreqTimer()
{
	m_square1.dutyIdx++;
	m_square1.dutyIdx &= 7;
	m_square1.frequency = (2048 - (SOUND1CNT_X & 0x7FF)) * 16;

	m_square1.output = ((dutyTable[m_square1.dutyPattern] >> (m_square1.dutyIdx)) & 0b1) ? 1 : -1;
	m_square1.output *= (m_square1.volume * 8);

	m_scheduler->addEvent(Event::Square1, &APU::square1EventCallback, (void*)this, m_scheduler->getEventTime() + m_square1.frequency);
}

void APU::onSquare2FreqTimer()
{
	m_square2.dutyIdx++;
	m_square2.dutyIdx &= 7;
	m_square2.frequency = (2048 - (SOUND2CNT_H & 0x7FF)) * 16;

	m_square2.output = ((dutyTable[m_square2.dutyPattern] >> (m_square2.dutyIdx)) & 0b1) ? 1 : -1;
	m_square2.output *= (m_square2.volume * 8);

	m_scheduler->addEvent(Event::Square2, &APU::square2EventCallback, (void*)this, m_scheduler->getEventTime() + m_square2.frequency);
}

void APU::updateDMAChannel(int channel)
{
	int baseEnableIdx = (channel == 0) ? 8 : 12;
	bool channelEnabled = ((SOUNDCNT_H >> baseEnableIdx) & 0b1) || ((SOUNDCNT_H >> (baseEnableIdx + 1)) & 0b1);	//see if fifo channel is enabled on either L/R output channels
	if (channelEnabled)
	{
		m_channels[channel].pop();
		if (m_channels[channel].size < 16)	//need to request more data!!
			m_shouldDMA = true;
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

void APU::square1EventCallback(void* context)
{
	APU* thisPtr = (APU*)context;
	thisPtr->onSquare1FreqTimer();
}

void APU::square2EventCallback(void* context)
{
	APU* thisPtr = (APU*)context;
	thisPtr->onSquare2FreqTimer();
}

float APU::highPass(float in, bool enable)
{
	if (!enable)
		return 0.0;
	float out = 0.0f;
	out = in - capacitor;
	capacitor = in - out * 0.996336;
	return out;
}