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
	desiredSpec.format = AUDIO_F32;	//might have to change this and make use of some actual resampling
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
	case 0x04000068:
		SOUND2CNT_L &= 0xFF00; SOUND2CNT_L |= value;
		break;
	case 0x04000069:
		SOUND2CNT_L &= 0xFF; SOUND2CNT_L |= (value << 8);
		break;
	case 0x0400006C:
		SOUND2CNT_H &= 0xFF00; SOUND2CNT_H |= value;
		break;
	case 0x0400006D:
		SOUND2CNT_H &= 0xFF; SOUND2CNT_H |= (value << 8);
		if ((value >> 7) & 0b1)	//reload channel
		{
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
		break;
	case 0x040000A4: case 0x040000A5: case 0x040000A6: case 0x040000A7:
		if (!m_channels[1].isFull())
			m_channels[1].push(value);
		break;
	}
}

void APU::onSampleEvent()
{
	int8_t chanASample = m_channels[0].currentSample;
	m_chanABuffer[sampleIndex] = (((float)chanASample) / 128.f);
	int8_t chanBSample = m_channels[1].currentSample;
	m_chanBBuffer[sampleIndex] = (((float)chanBSample) / 128.f);

	bool square2OutputEnabled = (((SOUNDCNT_L >> 9) & 0b1)) || (((SOUNDCNT_L >> 13)) & 0b1);
	int8_t square2Sample = m_square2.output >> (2 - (SOUNDCNT_H & 0b11));
	m_square2Buffer[sampleIndex] = highPass(square2Sample, square2OutputEnabled && m_square2.enabled);

	sampleIndex++;
	if (sampleIndex == sampleBufferSize)
	{
		sampleIndex = 0;
		float m_finalSamples[sampleBufferSize] = {};
		SDL_MixAudioFormat((uint8_t*)m_finalSamples, (uint8_t*)m_chanABuffer, AUDIO_F32, sampleBufferSize * 4, SDL_MIX_MAXVOLUME / 64);
		SDL_MixAudioFormat((uint8_t*)m_finalSamples, (uint8_t*)m_chanBBuffer, AUDIO_F32, sampleBufferSize * 4, SDL_MIX_MAXVOLUME / 64);
		//SDL_MixAudioFormat((uint8_t*)m_finalSamples, (uint8_t*)m_square2Buffer, AUDIO_F32, sampleBufferSize * 4, SDL_MIX_MAXVOLUME / 64);
		SDL_QueueAudio(m_audioDevice, (void*)m_finalSamples, sampleBufferSize*4);

		while (SDL_GetQueuedAudioSize(m_audioDevice) > sampleBufferSize * 4)
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

void APU::onSquare2FreqTimer()
{
	m_square2.dutyIdx++;
	m_square2.dutyIdx &= 7;
	m_square2.frequency = (2048 - (SOUND2CNT_H & 0x7FF)) * 16;

	m_square2.output = (dutyTable[m_square2.dutyPattern] >> (m_square2.dutyIdx)) & 0b1;
	m_square2.output *= (m_square2.volume>>1);

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