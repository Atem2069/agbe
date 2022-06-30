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
	desiredSpec.samples = 1024;		//1024 long sample buffer, we'll see how this goes
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
		{
			m_channels[0].empty();
			FIFODMACallback(dmaContext);
		}
		if ((SOUNDCNT_H >> 15) & 0b1)
		{
			m_channels[1].empty();
			FIFODMACallback(dmaContext);
		}
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
	//todo: audio fifo writes - and in addition the bits in SOUNDCNT_H that can reset the fifos
	}
}

void APU::onSampleEvent()
{

	int8_t chanASample = m_channels[0].currentSample;
	m_chanABuffer[sampleIndex] = (((float)chanASample) / 128.f);
	int8_t chanBSample = m_channels[1].currentSample;
	m_chanBBuffer[sampleIndex] = (((float)chanBSample) / 128.f);
	sampleIndex++;
	if (sampleIndex == 1024)
	{
		float m_finalSamples[1024] = {};
		SDL_MixAudioFormat((uint8_t*)m_finalSamples, (uint8_t*)m_chanABuffer, AUDIO_F32, 1024 * 4, SDL_MIX_MAXVOLUME / 64);
		SDL_MixAudioFormat((uint8_t*)m_finalSamples, (uint8_t*)m_chanBBuffer, AUDIO_F32, 1024 * 4, SDL_MIX_MAXVOLUME / 64);
		memset(m_chanABuffer, 0, 1024 * 4);
		memset(m_chanBBuffer, 0, 1024 * 4);
		sampleIndex = 0;
		m_channels[0].currentSample = 0;
		SDL_QueueAudio(m_audioDevice, (void*)m_finalSamples, 1024*4);
	}

	m_scheduler->addEvent(Event::AudioSample, &APU::sampleEventCallback, (void*)this, m_scheduler->getEventTime() + cyclesPerSample);
}

void APU::onTimer0Overflow()
{
	bool soundEnabled = ((SOUNDCNT_X >> 7) & 0b1);
	if (!soundEnabled)
		return;

	bool needToDMA = false;

	bool channelATimerSelect = ((SOUNDCNT_H >> 10) & 0b1);
	bool channelBTimerSelect = ((SOUNDCNT_H >> 14) & 0b1);
	bool chanAEnabled = ((SOUNDCNT_H >> 8) & 0b1) || ((SOUNDCNT_H >> 9) & 0b1);
	if (!channelATimerSelect && chanAEnabled)
	{
		m_channels[0].pop();
		if (m_channels[0].size <= 16)
			needToDMA = true;
	}
	bool chanBEnabled = ((SOUNDCNT_H >> 13 & 0b1) || ((SOUNDCNT_H >> 12) & 0b1));
	if (!channelBTimerSelect && chanBEnabled)
	{
		m_channels[1].pop();
		if (m_channels[1].size <= 16)
			needToDMA = true;
	}

	if (needToDMA)
		FIFODMACallback(dmaContext);
}

void APU::onTimer1Overflow()
{
	bool soundEnabled = ((SOUNDCNT_X >> 7) & 0b1);
	if (!soundEnabled)
		return;

	bool needToDMA = false;

	bool channelATimerSelect = ((SOUNDCNT_H >> 10) & 0b1);
	bool chanAEnabled = ((SOUNDCNT_H >> 8) & 0b1) || ((SOUNDCNT_H >> 9) & 0b1);
	bool channelBTimerSelect = ((SOUNDCNT_H >> 14) & 0b1);
	bool chanBEnabled = ((SOUNDCNT_H >> 13 & 0b1) || ((SOUNDCNT_H >> 12) & 0b1));
	if (channelATimerSelect && chanAEnabled)
	{
		m_channels[0].pop();
		if (m_channels[0].size <= 16)
			needToDMA = true;
	}
	if (channelBTimerSelect && chanBEnabled)
	{
		m_channels[1].pop();
		if (m_channels[1].size <= 16)
			needToDMA = true;
	}

	if (needToDMA)
		FIFODMACallback(dmaContext);
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

float APU::highPass(float in)
{
	float out = 0.0f;
	out = in - capacitor;
	capacitor = in - out * 0.996336;
	return out;
}