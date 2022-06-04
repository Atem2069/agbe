#pragma once

#include "Logger.h"
#include "GBAMem.h"
#include "PPU.h"
#include "Input.h"
#include "InterruptManager.h"

#include<iostream>

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

	uint16_t getValue16(uint8_t* arr, int base);
	void setValue16(uint8_t* arr, int base, uint16_t val);

	uint32_t getValue32(uint8_t* arr, int base);
	void setValue32(uint8_t* arr, int base, uint32_t val);
};