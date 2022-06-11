#pragma once

#include "Logger.h"
#include "GBAMem.h"
#include "PPU.h"
#include "Input.h"
#include "InterruptManager.h"
#include"Timer.h"

#include<iostream>

struct DMAChannel
{
	uint32_t srcAddress;
	uint32_t destAddress;
	uint16_t wordCount;
	uint16_t control;
};


class Bus
{
public:
	Bus(std::vector<uint8_t> BIOS, std::vector<uint8_t> cartData, std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<PPU> ppu, std::shared_ptr<Input> input);
	~Bus();

	void tick();	//inaccurate but tick 1 cycle per memory access

	uint8_t read8(uint32_t address, bool doTick=true);
	void write8(uint32_t address, uint8_t value, bool doTick=true);

	uint16_t read16(uint32_t address, bool doTick=true);
	void write16(uint32_t address, uint16_t value, bool doTick=true);

	uint32_t read32(uint32_t address, bool doTick=true);
	void write32(uint32_t address, uint32_t value, bool doTick=true);

	uint32_t fetch32(uint32_t address);
	uint16_t fetch16(uint32_t address);

//handle IO separately

	uint8_t readIO8(uint32_t address);
	void writeIO8(uint32_t address, uint8_t value);

	uint16_t readIO16(uint32_t address);
	void writeIO16(uint32_t address, uint16_t value);

	uint32_t readIO32(uint32_t address);
	void writeIO32(uint32_t address, uint32_t value);


private:
	std::shared_ptr<GBAMem> m_mem;
	std::shared_ptr<InterruptManager> m_interruptManager;
	std::shared_ptr<PPU> m_ppu;
	std::shared_ptr<Input> m_input;
	std::shared_ptr<Timer> m_timer;

	DMAChannel m_dmaChannels[4];
	uint16_t WAITCNT = 0;
	uint16_t hack_soundbias = 0;

	bool biosLockout = false;
	bool dmaInProgress = false;

	uint16_t getValue16(uint8_t* arr, int base, int mask);
	void setValue16(uint8_t* arr, int base, int mask, uint16_t val);

	uint32_t getValue32(uint8_t* arr, int base, int mask);
	void setValue32(uint8_t* arr, int base, int mask, uint32_t val);

	void setByteInWord(uint32_t* word, uint8_t byte, int pos);
	void setByteInHalfword(uint16_t* halfword, uint8_t byte, int pos);

	uint8_t DMARegRead(uint32_t address);
	void DMARegWrite(uint32_t address, uint8_t value);
	void checkDMAChannels();
	void doDMATransfer(int channel);
};