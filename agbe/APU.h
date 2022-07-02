#pragma once

#include"Logger.h"
#include"Scheduler.h"

#include<iostream>
#include<SDL.h>
#undef main			//really sdl??

struct AudioFIFO
{
	int8_t data[32];
	int startIdx = 0;
	int endIdx = 0;
	int size = 0;

	void push(int8_t val)
	{
		data[endIdx] = val;
		endIdx = (endIdx + 1) % 32;	//limit range to 0->31
		size++;
	}
	void pop()
	{
		int8_t retVal = data[startIdx];
		startIdx = (startIdx + 1) % 32;
		size--;
		currentSample = retVal;
	}
	void empty()
	{
		startIdx = 0; endIdx = 0;
		size = 0;
	}

	bool isEmpty()
	{
		return size == 0;
	}

	bool isFull()
	{
		return size >= 32;
	}

	int8_t currentSample = 0;	//holds last sample from timer event
};

struct SquareChannel2
{
	bool enabled;
	bool doLength;
	int lengthCounter;
	int dutyPattern;
	int frequency;
	int dutyIdx;
	int volume;
	int8_t output;
};

class APU
{
public:
	APU(std::shared_ptr<Scheduler> scheduler);
	~APU();

	void registerDMACallback(callbackFn dmaCallback, void* context);

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);

	static void sampleEventCallback(void* context);
	static void square2EventCallback(void* context);
	static void timer0Callback(void* context);
	static void timer1Callback(void* context);
private:
	std::shared_ptr<Scheduler> m_scheduler;
	AudioFIFO m_channels[2];
	SquareChannel2 m_square2 = {};

	uint16_t SOUNDCNT_L = {};
	uint16_t SOUNDCNT_H = {};
	uint8_t SOUNDCNT_X = {};
	uint16_t SOUNDBIAS = {};
	uint16_t SOUND2CNT_L = {};
	uint16_t SOUND2CNT_H = {};

	static constexpr int cyclesPerSample = 256;	//~64KHz sample rate, so we want to mix samples together roughly every that many cycles
	static constexpr int sampleRate = 65536;
	static constexpr int sampleBufferSize = 2048;

	static constexpr uint8_t dutyTable[4] =
	{
		0b00000001,
		0b00000011,
		0b00001111,
		0b11111100
	};

	bool m_shouldDMA = false;
	void updateDMAChannel(int channel);

	callbackFn FIFODMACallback;
	void* dmaContext;

	void onSampleEvent();
	void onTimer0Overflow();
	void onTimer1Overflow();
	void onSquare2FreqTimer();

	SDL_AudioDeviceID m_audioDevice = {};
	int16_t m_sampleBuffer[sampleBufferSize] = {};
	int sampleIndex = 0;

	float capacitor = 0.0f;
	float highPass(float in, bool enable);
};