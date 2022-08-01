#pragma once

#include"Logger.h"
#include"Scheduler.h"

#include<iostream>
#include<SDL.h>
#undef main			//really sdl??

typedef void(*FIFOcallbackFn)(void*, int);

struct AudioFIFO
{
	uint32_t data[7];
	uint32_t inFlightWord=0;
	int startIdx = 0;
	int endIdx = 0;
	int size = 0;

	bool samplePlaying = false;
	int playbackPosition = 0;

	void pushSample(uint8_t value, int offs)
	{
		if (isFull())
			return;
		uint32_t curWord = data[endIdx];
		uint32_t mask = ~(0xFF << (offs * 8));
		curWord &= mask;
		curWord |= (value << (offs * 8));
		data[endIdx] = curWord;
	}

	void advanceSamplePtr()
	{
		if (isFull())
			return;
		endIdx = (endIdx + 1);
		if (endIdx == 7)
			endIdx = 0;
		size++;
	}

	void copyNewWord()
	{
		if (!size)
			return;
		samplePlaying = true;
		uint32_t retVal = data[startIdx];
		startIdx = (startIdx + 1);
		if (startIdx == 7)
			startIdx = 0;
		size--;
		inFlightWord = retVal;
	}

	void popSample()
	{
		if (!samplePlaying || playbackPosition==4)
		{
			samplePlaying = false;
			playbackPosition = 0;
			copyNewWord();
		}

		if (samplePlaying)
		{
			currentSample = (inFlightWord >> (playbackPosition * 8)) & 0xFF;
			playbackPosition++;
		}
	}

	void empty()
	{
		samplePlaying = false;
		startIdx = 0; endIdx = 0;
		size = 0;
	}

	bool isEmpty()
	{
		return size == 0;
	}

	bool isFull()
	{
		return size >= 7;
	}

	int8_t currentSample = 0;	//holds last sample from timer event
};

struct SquareChannel1
{
	bool enabled;
	bool doLength;
	int lengthCounter;
	int dutyPattern;
	int frequency;
	int dutyIdx;
	int16_t output;

	//envelope
	int volume;
	int envelopePeriod;
	int envelopeTimer;
	bool envelopeIncrease;

	//chan 1 specific stuff
	bool sweepEnabled;
	int shadowFrequency;
	int sweepPeriod;
	int sweepTimer;
	int sweepShift;
	bool sweepNegate;

	uint64_t lastCheckTimestamp;
};

struct SquareChannel2
{
	bool enabled;
	bool doLength;
	int lengthCounter;
	int dutyPattern;
	int frequency;
	int dutyIdx;
	int16_t output;

	//envelope
	int volume;
	int envelopePeriod;
	int envelopeTimer;
	bool envelopeIncrease;

	uint64_t lastCheckTimestamp;
};

struct WaveChannel
{
	bool enabled;
	bool canEnable;
	bool doLength;
	int lengthCounter;
	int frequency;
	int sampleIndex;
	int volumeCode;
	int16_t output;

	bool twoDimensionBanking;
	bool currentBankNumber;
	uint8_t waveRam[2][16];	//two banks of wave ram, each holds 32 4 bit samples

	uint64_t lastCheckTimestamp;
};

struct NoiseChannel
{
	bool enabled;
	bool doLength;
	int lengthCounter;
	int frequency;
	uint16_t LFSR;
	int divisorCode;
	bool widthMode;
	int shiftAmount;
	int16_t output;

	//envelope
	int volume;
	int envelopePeriod;
	int envelopeTimer;
	bool envelopeIncrease;

	uint64_t lastCheckTimestamp;
};

class APU
{
public:
	APU(std::shared_ptr<Scheduler> scheduler);
	~APU();

	void registerDMACallback(FIFOcallbackFn dmaCallback, void* context);

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);

	static void sampleEventCallback(void* context);
	static void frameSequencerCallback(void* context);
	static void timer0Callback(void* context);
	static void timer1Callback(void* context);

	void advanceSamplePtr(int channel);
private:
	std::shared_ptr<Scheduler> m_scheduler;
	AudioFIFO m_channels[2];

	SquareChannel1 m_square1 = {};
	SquareChannel2 m_square2 = {};
	WaveChannel m_waveChannel = {};
	NoiseChannel m_noiseChannel = {};

	void triggerSquare1();
	void triggerSquare2();
	void triggerWave();
	void triggerNoise();

	uint16_t SOUNDCNT_L = {};
	uint16_t SOUNDCNT_H = {};
	uint8_t SOUNDCNT_X = {};
	uint16_t SOUNDBIAS = {};

	uint8_t SOUND1CNT_L = {};
	uint16_t SOUND1CNT_H = {};
	uint16_t SOUND1CNT_X = {};

	uint16_t SOUND2CNT_L = {};
	uint16_t SOUND2CNT_H = {};

	uint8_t SOUND3CNT_L = {};
	uint16_t SOUND3CNT_H = {};
	uint16_t SOUND3CNT_X = {};

	uint16_t SOUND4CNT_L = {};
	uint16_t SOUND4CNT_H = {};

	static constexpr int cyclesPerSample = 256;	//~64KHz sample rate, so we want to mix samples together roughly every that many cycles
	static constexpr int sampleRate = 65536;
	static constexpr int sampleBufferSize = 2048;

	static constexpr uint8_t dutyTable[4] =		//fixed duty table for square wave channels
	{
		0b00000001,
		0b00000011,
		0b00001111,
		0b11111100
	};

	static constexpr int divisorMappings[8] = { 8,16,32,48,64,80,96,112 };	//divisor mappings for calculating noise channel frequency

	void updateDMAChannel(int channel);

	FIFOcallbackFn FIFODMACallback;
	void* dmaContext;

	void onSampleEvent();
	void onTimer0Overflow();
	void onTimer1Overflow();
	void updateSquare1();
	void updateSquare2();
	void updateWave();
	void updateNoise();
	void onFrameSequencerEvent();

	//Frameseq related stuff
	int frameSequencerClock = 0;

	void clockLengthCounters();
	void clockVolumeEnvelope();
	void clockFrequencySweep();
	int calcChan1Frequency();

	void resetAllChannels();


	SDL_AudioDeviceID m_audioDevice = {};
	float m_sampleBuffer[sampleBufferSize*2] = {};
	int sampleIndex = 0;

	float clipSample(int16_t sampleIn);
	float capacitor = 0.0f;
	float highPass(float in);
	void lowPass(float* outBuffer, float* inBuffer);
};