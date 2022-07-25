#include"APU.h"

APU::APU(std::shared_ptr<Scheduler> scheduler)
{
	for (int i = 0; i < 2; i++)
		m_channels[i].empty();

	m_scheduler = scheduler;
	m_scheduler->addEvent(Event::AudioSample, &APU::sampleEventCallback, (void*)this, cyclesPerSample);
	m_scheduler->addEvent(Event::FrameSequencer, &APU::frameSequencerCallback, (void*)this, 32768);

	SDL_Init(SDL_INIT_AUDIO);
	SDL_AudioSpec desiredSpec = {}, obtainedSpec = {};
	desiredSpec.freq = sampleRate;
	desiredSpec.format = AUDIO_S16;	
	desiredSpec.channels = 2;		
	desiredSpec.silence = 0;
	desiredSpec.samples = sampleBufferSize;	
	m_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, &obtainedSpec, 0);
	SDL_PauseAudioDevice(m_audioDevice, 0);

	m_noiseChannel.LFSR = 0xFFFF;
}

APU::~APU()
{
	SDL_Quit();
}

void APU::registerDMACallback(FIFOcallbackFn dmaCallback, void* context)
{
	FIFODMACallback = dmaCallback;
	dmaContext = context;
}

uint8_t APU::readIO(uint32_t address)
{
	switch (address)
	{
	case 0x04000060:
		return SOUND1CNT_L & 0x7F;
	case 0x04000062:
		return SOUND1CNT_H & 0xC0;
	case 0x04000063:
		return ((SOUND1CNT_H >> 8) & 0xFF);
	case 0x04000065:
		return ((SOUND1CNT_X >> 8) & 0x40);
	case 0x04000068:
		return SOUND2CNT_L & 0xC0;
	case 0x04000069:
		return ((SOUND2CNT_L >> 8) & 0xFF);
	case 0x0400006D:
		return ((SOUND2CNT_H >> 8) & 0x40);
	case 0x04000070:
		return SOUND3CNT_L & 0xE0;
	case 0x04000073:
		return ((SOUND3CNT_H >> 8) & 0xE0);
	case 0x04000075:
		return ((SOUND3CNT_X >> 8) & 0x40);
	case 0x04000079:
		return ((SOUND4CNT_L >> 8) & 0xFF);
	case 0x0400007C:
		return (SOUND4CNT_H & 0xFF);
	case 0x0400007D:
		return ((SOUND4CNT_H >> 8) & 0x40);
	case 0x04000080:
		return (SOUNDCNT_L & 0x77);
	case 0x04000081:
		return ((SOUNDCNT_L >> 8) & 0xFF);
	case 0x04000082:
		return (SOUNDCNT_H & 0x0F);
	case 0x04000083:
		return ((SOUNDCNT_H >> 8) & 0x77);
	case 0x04000084:
		return SOUNDCNT_X & 0x8F;
	case 0x04000088:
		return SOUNDBIAS & 0xFE;
	case 0x04000089:
		return ((SOUNDBIAS >> 8) & 0xC3);

	case 0x04000090: case 0x04000091: case 0x04000092: case 0x04000093: case 0x04000094: case 0x04000095: case 0x04000096: case 0x04000097:
	case 0x04000098: case 0x04000099: case 0x0400009A: case 0x0400009B: case 0x0400009C: case 0x0400009D: case 0x0400009E: case 0x0400009F:
		return m_waveChannel.waveRam[!m_waveChannel.currentBankNumber][address - 0x04000090];
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
		m_square1.lengthCounter = 64 - (SOUND1CNT_H & 0x3F);
		m_square1.envelopePeriod = ((SOUND1CNT_H >> 8) & 0b111);
		m_square1.envelopeTimer = m_square1.envelopePeriod;
		m_square1.envelopeIncrease = ((SOUND1CNT_H >> 11) & 0b1);
		m_square1.volume = ((SOUND1CNT_H >> 12) & 0xF);
		break;
	case 0x04000064:
		SOUND1CNT_X &= 0xFF00; SOUND1CNT_X |= value;
		break;
	case 0x04000065:
		SOUND1CNT_X &= 0xFF; SOUND1CNT_X |= (value << 8);
		if ((value >> 7) & 0b1)
			triggerSquare1();
		break;
	case 0x04000068:
		SOUND2CNT_L &= 0xFF00; SOUND2CNT_L |= value;
		break;
	case 0x04000069:
		SOUND2CNT_L &= 0xFF; SOUND2CNT_L |= (value << 8);
		m_square2.lengthCounter = 64 - (SOUND2CNT_L & 0x3F);
		m_square2.envelopePeriod = ((SOUND2CNT_L >> 8) & 0b111);
		m_square2.envelopeTimer = m_square2.envelopePeriod;
		m_square2.envelopeIncrease = ((SOUND2CNT_L >> 11) & 0b1);
		m_square2.volume = ((SOUND2CNT_L >> 12) & 0xF);
		break;
	case 0x0400006C:
		SOUND2CNT_H &= 0xFF00; SOUND2CNT_H |= value;
		break;
	case 0x0400006D:
		SOUND2CNT_H &= 0xFF; SOUND2CNT_H |= (value << 8);
		if ((value >> 7) & 0b1)	//reload channel
			triggerSquare2();
		break;
	case 0x04000070:
		SOUND3CNT_L = value;
		m_waveChannel.twoDimensionBanking = ((value >> 5) & 0b1);
		m_waveChannel.currentBankNumber = ((value >> 6) & 0b1);
		m_waveChannel.canEnable = ((value >> 7) & 0b1);	//<-- enable bit
		if (!m_waveChannel.canEnable)
		{
			m_waveChannel.enabled = false;
			SOUNDCNT_X &= ~0b100;
		}
		break;
	case 0x04000072:
		SOUND3CNT_H &= 0xFF00; SOUND3CNT_H |= value;
		break;
	case 0x04000073:
		SOUND3CNT_H &= 0xFF; SOUND3CNT_H |= (value << 8);
		m_waveChannel.lengthCounter = 256 - (SOUND3CNT_H & 0xFF);
		m_waveChannel.volumeCode = ((SOUND3CNT_H >> 13) & 0b11);
		//THIS IS A HACK: should be 75% of volume
		if ((SOUND3CNT_H >> 15) & 0b1)
			m_waveChannel.volumeCode = 1;
		break;
	case 0x04000074:
		SOUND3CNT_X &= 0xFF00; SOUND3CNT_X |= value;
		break;
	case 0x04000075:
		SOUND3CNT_X &= 0xFF; SOUND3CNT_X |= (value << 8);
		if (((value >> 7) & 0b1) && m_waveChannel.canEnable)
			triggerWave();
		break;
	case 0x04000078:
		SOUND4CNT_L &= 0xFF00; SOUND4CNT_L |= value;
		break;
	case 0x04000079:
		SOUND4CNT_L &= 0xFF; SOUND4CNT_L |= (value << 8);
		m_noiseChannel.lengthCounter = 64 - (SOUND4CNT_L & 0x3F);
		m_noiseChannel.envelopePeriod = ((SOUND4CNT_L >> 8) & 0b111);
		m_noiseChannel.envelopeTimer = m_noiseChannel.envelopePeriod;
		m_noiseChannel.envelopeIncrease = ((SOUND4CNT_L >> 11) & 0b1);
		m_noiseChannel.volume = ((SOUND4CNT_L >> 12) & 0xF);
		break;
	case 0x0400007C:
		SOUND4CNT_H &= 0xFF00; SOUND4CNT_H |= value;
		m_noiseChannel.divisorCode = SOUND4CNT_H & 0b111;
		m_noiseChannel.widthMode = ((SOUND4CNT_H >> 3) & 0b1);
		m_noiseChannel.shiftAmount = ((SOUND4CNT_H >> 4) & 0xF);
		break;
	case 0x0400007D:
		SOUND4CNT_H &= 0xFF; SOUND4CNT_H |= (value << 8);
		if ((value >> 7) & 0b1)
			triggerNoise();
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
		if (!((SOUNDCNT_X >> 7) & 0b1))
			resetAllChannels();
		break;
	case 0x04000088:
		SOUNDBIAS &= 0xFF00; SOUNDBIAS |= value;
		break;
	case 0x04000089:
		SOUNDBIAS &= 0xFF; SOUNDBIAS |= (value << 8);
		break;
	case 0x040000A0: case 0x040000A1: case 0x040000A2: case 0x040000A3:			
		m_channels[0].pushSample(value, address & 3);
		break;
	case 0x040000A4: case 0x040000A5: case 0x040000A6: case 0x040000A7:
		m_channels[1].pushSample(value, address & 3);
		break;

	case 0x04000090: case 0x04000091: case 0x04000092: case 0x04000093: case 0x04000094: case 0x04000095: case 0x04000096: case 0x04000097:
	case 0x04000098: case 0x04000099: case 0x0400009A: case 0x0400009B: case 0x0400009C: case 0x0400009D: case 0x0400009E: case 0x0400009F:
		m_waveChannel.waveRam[!m_waveChannel.currentBankNumber][address - 0x04000090] = value;
		break;
	}
}

void APU::onSampleEvent()
{
	updateSquare1();
	updateSquare2();
	updateWave();
	updateNoise();

	int16_t chanASample = m_channels[0].currentSample << 1+((SOUNDCNT_H  >> 2) & 0b1);		//applies sound a/b volume. 100% = left shift by 1 (*2), 50% = no change
	int16_t chanBSample = m_channels[1].currentSample << 1+((SOUNDCNT_H >> 3) & 0b1);

	int psgVolumeShift = (2 - (SOUNDCNT_H & 0b11));	//shift applied to all psg channels

	int16_t square1Sample = m_square1.output >> psgVolumeShift;
	int16_t square2Sample = m_square2.output >> psgVolumeShift;
	int16_t waveSample = m_waveChannel.output >> psgVolumeShift;
	int16_t noiseSample = m_noiseChannel.output >> psgVolumeShift;

	//both of these are messy - but it extracts L/R enable bits to see if each channel is enabled for each output (L/R)
	int16_t leftSample = (((SOUNDCNT_H >> 9) & 0b1) * chanASample) + (((SOUNDCNT_H >> 13) & 0b1) * chanBSample) + (((SOUNDCNT_L >> 12) & 0b1) * square1Sample)
		+ (((SOUNDCNT_L >> 13) & 0b1) * square2Sample) + (((SOUNDCNT_L >> 14) & 0b1) * waveSample) + (((SOUNDCNT_L >> 15) & 0b1) * noiseSample);
	leftSample <<= 2;

	int16_t rightSample = (((SOUNDCNT_H >> 8) & 0b1) * chanASample) + (((SOUNDCNT_H >> 12) & 0b1) * chanBSample) + (((SOUNDCNT_L >> 8) & 0b1) * square1Sample)
		+ (((SOUNDCNT_L >> 9) & 0b1) * square2Sample) + (((SOUNDCNT_L >> 10) & 0b1) * waveSample) + (((SOUNDCNT_L >> 11) & 0b1) * noiseSample);
	rightSample <<= 2;
	m_sampleBuffer[sampleIndex << 1] = leftSample;
	m_sampleBuffer[(sampleIndex << 1) | 1] = rightSample;

	sampleIndex++;
	if (sampleIndex == sampleBufferSize)
	{
		sampleIndex = 0;
		//memcpy((void*)m_finalSamples, (void*)m_sampleBuffer, sampleBufferSize * 4);
		int16_t m_finalSamples[sampleBufferSize * 2] = {};
		lowPass(m_finalSamples, m_sampleBuffer);
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
}

void APU::updateSquare1()
{
	if (!m_square1.enabled)
	{
		m_square1.output = 0;
		return;
	}
	uint64_t curTime = m_scheduler->getCurrentTimestamp();
	uint64_t timeDiff = curTime - m_square1.lastCheckTimestamp;
	while (timeDiff >= m_square1.frequency)
	{
		if (m_square1.frequency == 0)
			break;
		timeDiff -= m_square1.frequency;
		m_square1.dutyIdx++;
		m_square1.dutyIdx &= 7;
		m_square1.frequency = (2048 - (SOUND1CNT_X & 0x7FF)) * 16;
		m_square1.output = 0;
		if (m_square1.enabled)
		{
			m_square1.output = ((dutyTable[m_square1.dutyPattern] >> (m_square1.dutyIdx)) & 0b1) ? 1 : -1;
			m_square1.output *= (m_square1.volume * 8);
		}
	}
	m_square1.frequency -= timeDiff;
	m_square1.lastCheckTimestamp = curTime;
}

void APU::updateSquare2()
{
	if (!m_square2.enabled)
	{
		m_square2.output = 0;
		return;
	}
	uint64_t curTime = m_scheduler->getCurrentTimestamp();
	uint64_t timeDiff = curTime - m_square2.lastCheckTimestamp;
	while (timeDiff >= m_square2.frequency)
	{
		if (m_square2.frequency == 0)
			break;
		timeDiff -= m_square2.frequency;
		m_square2.dutyIdx++;
		m_square2.dutyIdx &= 7;
		m_square2.frequency = (2048 - (SOUND2CNT_H & 0x7FF)) * 16;
		m_square2.output = 0;
		if (m_square2.enabled)
		{
			m_square2.output = ((dutyTable[m_square2.dutyPattern] >> (m_square2.dutyIdx)) & 0b1) ? 1 : -1;
			m_square2.output *= (m_square2.volume * 8);
		}
	}
	m_square2.frequency -= timeDiff;
	m_square2.lastCheckTimestamp = curTime;
}

void APU::updateWave()
{
	if (!m_waveChannel.enabled)
	{
		m_waveChannel.output = 0;
		return;
	}
	uint64_t curTime = m_scheduler->getCurrentTimestamp();
	uint64_t timeDiff = curTime - m_waveChannel.lastCheckTimestamp;
	while (timeDiff >= m_waveChannel.frequency)
	{
		if (m_waveChannel.frequency == 0)
			break;
		timeDiff -= m_waveChannel.frequency;
		m_waveChannel.frequency = (2048 - (SOUND3CNT_X & 0x7FF)) * 8;
		m_waveChannel.output = 0;
		if (m_waveChannel.enabled)
		{
			int8_t byteRow = m_waveChannel.waveRam[m_waveChannel.currentBankNumber][m_waveChannel.sampleIndex >> 1];
			int8_t sample = 0;
			if (m_waveChannel.sampleIndex & 0b1)	//can potentially remove this if with something cleaner
				sample = byteRow & 0xF;
			else
				sample = (byteRow >> 4) & 0xF;

			switch (m_waveChannel.volumeCode)
			{
			case 0:
				sample = 0;
				break;
			case 2:
				sample >>= 1;
				break;
			case 3:
				sample >>= 2;
				break;
			}

			m_waveChannel.output = sample * 16;
		}

		m_waveChannel.sampleIndex = (m_waveChannel.sampleIndex + 1) & 31;
		if (m_waveChannel.sampleIndex == 0 && m_waveChannel.twoDimensionBanking)
			m_waveChannel.currentBankNumber = !m_waveChannel.currentBankNumber;
	}
	m_waveChannel.frequency -= timeDiff;
	m_waveChannel.lastCheckTimestamp = curTime;
}

void APU::updateNoise()
{
	if (!m_noiseChannel.enabled)
	{
		m_noiseChannel.output = 0;
		return;
	}
	uint64_t curTime = m_scheduler->getCurrentTimestamp();
	uint64_t timeDiff = curTime - m_noiseChannel.lastCheckTimestamp;
	while (timeDiff >= m_noiseChannel.frequency)
	{
		if (m_noiseChannel.frequency == 0)
			break;
		timeDiff -= m_noiseChannel.frequency;
		int divisor = divisorMappings[m_noiseChannel.divisorCode];
		m_noiseChannel.frequency = divisor << (m_noiseChannel.shiftAmount + 2);	//same situation with the '+2'
		m_noiseChannel.output = 0;
		if (m_noiseChannel.enabled)
		{
			bool wasCarry = m_noiseChannel.LFSR & 0b1;
			m_noiseChannel.LFSR >>= 1;

			m_noiseChannel.output = (wasCarry) ? 1 : -1;
			m_noiseChannel.output *= m_noiseChannel.volume * 8;

			if (wasCarry)
			{
				if (m_noiseChannel.widthMode)
					m_noiseChannel.LFSR ^= 0x60;
				else
					m_noiseChannel.LFSR ^= 0x6000;
			}
			m_noiseChannel.LFSR &= 0x7FFF;
			if (m_noiseChannel.widthMode)
				m_noiseChannel.LFSR &= 0x7F;
		}
	}
	m_noiseChannel.frequency -= timeDiff;
	m_noiseChannel.lastCheckTimestamp = curTime;
}

void APU::onFrameSequencerEvent()
{
	if ((frameSequencerClock & 1) == 0)
		clockLengthCounters();
	if ((frameSequencerClock & 7) == 7)
		clockVolumeEnvelope();
	if ((frameSequencerClock & 3) == 2)
		clockFrequencySweep();

	frameSequencerClock = (frameSequencerClock + 1) & 7;
	m_scheduler->addEvent(Event::FrameSequencer, &APU::frameSequencerCallback, (void*)this, m_scheduler->getEventTime() + 32768);
}

void APU::clockLengthCounters()
{
	if (m_square1.doLength)
	{
		if (m_square1.lengthCounter > 0)
			m_square1.lengthCounter--;
		if (m_square1.lengthCounter == 0)
		{
			m_square1.enabled = false;
			SOUNDCNT_X &= ~0b1;
		}
	}

	if (m_square2.doLength)
	{
		if (m_square2.lengthCounter > 0)
			m_square2.lengthCounter--;
		if (m_square2.lengthCounter == 0)
		{
			m_square2.enabled = false;
			SOUNDCNT_X &= ~0b10;
		}
	}

	if (m_waveChannel.doLength)
	{
		if (m_waveChannel.lengthCounter > 0)
			m_waveChannel.lengthCounter--;
		if (m_waveChannel.lengthCounter == 0)
		{
			m_waveChannel.enabled = false;
			SOUNDCNT_X &= ~0b100;
		}
	}

	if (m_noiseChannel.doLength)
	{
		if (m_noiseChannel.lengthCounter > 0)
			m_noiseChannel.lengthCounter--;
		if (m_noiseChannel.lengthCounter == 0)
		{
			m_noiseChannel.enabled = false;
			SOUNDCNT_X &= ~0b1000;
		}
	}
}

void APU::clockVolumeEnvelope()
{
	if (m_square1.enabled)
	{
		if (m_square1.envelopePeriod != 0)
		{
			m_square1.envelopeTimer--;
			if (m_square1.envelopeTimer == 0)
			{
				m_square1.envelopeTimer = m_square1.envelopePeriod;
				if (m_square1.envelopeIncrease && m_square1.volume < 15)
					m_square1.volume++;
				if (!m_square1.envelopeIncrease && m_square1.volume > 0)
					m_square1.volume--;
			}
		}
	}

	if (m_square2.enabled)
	{
		if (m_square2.envelopePeriod != 0)
		{
			m_square2.envelopeTimer--;
			if (m_square2.envelopeTimer == 0)
			{
				m_square2.envelopeTimer = m_square2.envelopePeriod;
				if (m_square2.envelopeIncrease && m_square2.volume < 15)
					m_square2.volume++;
				if (!m_square2.envelopeIncrease && m_square2.volume > 0)
					m_square2.volume--;
			}
		}
	}

	if (m_noiseChannel.enabled)
	{
		if (m_noiseChannel.envelopePeriod != 0)
		{
			m_noiseChannel.envelopeTimer--;
			if (m_noiseChannel.envelopeTimer == 0)
			{
				m_noiseChannel.envelopeTimer = m_noiseChannel.envelopePeriod;
				if (m_noiseChannel.envelopeIncrease && m_noiseChannel.volume < 15)
					m_noiseChannel.volume++;
				if (!m_noiseChannel.envelopeIncrease && m_noiseChannel.volume > 0)
					m_noiseChannel.volume--;
			}
		}
	}
}

void APU::clockFrequencySweep()
{
	//this code is disgusting: not sure why i wrote it like this but it's how my gb emu does it :P. 

	if (m_square1.sweepPeriod != 0)
	{
		if (m_square1.sweepTimer > 0)
			m_square1.sweepTimer--;

		if (m_square1.sweepTimer == 0)
		{
			m_square1.sweepTimer = m_square1.sweepPeriod;
			
			int oldFreq = SOUND1CNT_X & 0x7FF;
			int newFreq = oldFreq >> m_square1.sweepShift;
			if (m_square1.sweepNegate)
				newFreq = oldFreq - newFreq;
			else
				newFreq = oldFreq + newFreq;

			if (newFreq > 2047)				//overflow disable channel
			{
				m_square1.enabled = false;
				SOUNDCNT_X &= ~0b1;
			}
			else
			{
				SOUND1CNT_X &= ~0x7FF;
				SOUND1CNT_X |= (newFreq & 0x7FF);	//<----wtf? why did i do this?
			}
		}
	}
}

void APU::resetAllChannels()
{
	m_square1 = {};
	m_square2 = {};
	m_waveChannel = {};
	m_noiseChannel = {};
	SOUNDCNT_X &= ~0xF;	//clear PSG channel enable bits

	//all PSG channel registers now reset to 0
	SOUNDCNT_L = {};

	SOUND1CNT_L = {};
	SOUND1CNT_H = {};
	SOUND1CNT_X = {};

	SOUND2CNT_L = {};
	SOUND2CNT_H = {};

	SOUND3CNT_L = {};
	SOUND3CNT_H = {};
	SOUND3CNT_X = {};

	SOUND4CNT_L = {};
	SOUND4CNT_H = {};
}

void APU::updateDMAChannel(int channel)
{
	int baseEnableIdx = (channel == 0) ? 8 : 12;
	bool channelEnabled = ((SOUNDCNT_H >> baseEnableIdx) & 0b1) || ((SOUNDCNT_H >> (baseEnableIdx + 1)) & 0b1);	//see if fifo channel is enabled on either L/R output channels
	if (channelEnabled)
	{
		if (m_channels[channel].size <= 3)	//need to request more data!!
			FIFODMACallback(dmaContext, channel);
		m_channels[channel].popSample();
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

void APU::frameSequencerCallback(void* context)
{
	APU* thisPtr = (APU*)context;
	thisPtr->onFrameSequencerEvent();
}

void APU::triggerSquare1()
{
	SOUNDCNT_X |= 0b1;

	if (m_square1.lengthCounter == 0)
		m_square1.lengthCounter = 64;
	m_square1.frequency = (2048 - (SOUND1CNT_X & 0x7FF)) * 16;
	m_square1.enabled = true;
	m_square1.dutyPattern = (SOUND1CNT_H >> 6) & 0b11;
	m_square1.doLength = ((SOUND1CNT_X >> 14) & 0b1);
	m_square1.envelopeTimer = m_square1.envelopePeriod;
	m_square1.volume = ((SOUND1CNT_H >> 12) & 0xF);
	m_square1.lastCheckTimestamp = m_scheduler->getCurrentTimestamp();
}

void APU::triggerSquare2()
{
	SOUNDCNT_X |= 0b10;	//reenable square 2 in soundcnt_x

	if (m_square2.lengthCounter == 0)
		m_square2.lengthCounter = 64;
	m_square2.frequency = (2048 - (SOUND2CNT_H & 0x7FF)) * 16;
	m_square2.enabled = true;
	m_square2.dutyPattern = (SOUND2CNT_L >> 6) & 0b11;
	m_square2.doLength = ((SOUND2CNT_H >> 14) & 0b1);
	m_square2.envelopeTimer = m_square2.envelopePeriod;
	m_square2.volume = ((SOUND2CNT_L >> 12) & 0xF);
	m_square2.lastCheckTimestamp = m_scheduler->getCurrentTimestamp();
}

void APU::triggerWave()
{
	SOUNDCNT_X |= 0b100;
	m_waveChannel.enabled = true;

	if (m_waveChannel.lengthCounter == 0)
		m_waveChannel.lengthCounter = 256;
	m_waveChannel.frequency = (2048 - (SOUND3CNT_X & 0x7FF)) * 8;
	m_waveChannel.sampleIndex = 0;
	m_waveChannel.currentBankNumber = (SOUND3CNT_L >> 6) & 0b1;	//todo: doublecheck if this is right;
	m_waveChannel.doLength = ((SOUND3CNT_X >> 14) & 0b1);
	m_waveChannel.lastCheckTimestamp = m_scheduler->getCurrentTimestamp();
}

void APU::triggerNoise()
{
	SOUNDCNT_X |= 0b1000;
	m_noiseChannel.enabled = true;

	if (m_noiseChannel.lengthCounter == 0)
		m_noiseChannel.lengthCounter = 64;
	m_noiseChannel.doLength = ((SOUND4CNT_H >> 14) & 0b1);
	m_noiseChannel.LFSR = 0x40;
	if (!m_noiseChannel.widthMode)
		m_noiseChannel.LFSR <<= 8;

	int divisor = divisorMappings[m_noiseChannel.divisorCode];
	m_noiseChannel.frequency = divisor << (m_noiseChannel.shiftAmount + 2);	//<-- +2 accounts for us measuring cycles at 16.7MHz
	m_noiseChannel.lastCheckTimestamp = m_scheduler->getCurrentTimestamp();
}

void APU::lowPass(int16_t* outBuffer, int16_t* inBuffer)
{
	float dt = 1.0 / (float)sampleRate;
	float a = (2.0 * 3.14 * 20000*dt) / ((2.0 * 3.14 * 20000*dt) + 1);	//20k cutoff for high pass
	outBuffer[0] = a *inBuffer[0];
	outBuffer[1] = a * inBuffer[1];
	for (int i = 1; i < sampleBufferSize; i++)
	{
		float curSample = inBuffer[i * 2];
		outBuffer[i * 2] = (float)outBuffer[(i * 2) - 2] + a * (curSample - (float)outBuffer[(i * 2) - 2]);

		curSample = inBuffer[(i * 2) + 1];
		outBuffer[(i * 2) + 1] = (float)outBuffer[(i * 2) - 1] + a * (curSample - (float)outBuffer[(i * 2) - 1]);
	}
}

void APU::advanceSamplePtr(int channel)
{
	m_channels[channel].advanceSamplePtr();
}