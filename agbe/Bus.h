#pragma once

#include "Logger.h"
#include "GBAMem.h"
#include "PPU.h"
#include "Input.h"
#include "InterruptManager.h"
#include"Timer.h"
#include"EEPROM.h"
#include"Flash.h"
#include"SRAM.h"
#include"Scheduler.h"
#include"APU.h"
#include"SerialStub.h"

#include<iostream>

struct DMAChannel
{
	uint32_t srcAddress;
	uint32_t destAddress;
	uint32_t internalDest;
	uint32_t internalSrc;
	uint16_t wordCount;
	uint16_t control;
};

struct OpenBus
{
	uint32_t bios;	//bios open bus value
	uint32_t mem;	//open bus for other unused mem
	uint32_t dma;
	bool dmaJustFinished = false;	//weird flag :P override open bus with this, if dma 'just' finished
};

enum class AccessType
{
	Nonsequential=0,
	Sequential=1,
	Prefetch				//this access takes zero waitstates to complete (we assume those waitstates already ticked in i-cycles or mem accesses)
};

class Bus
{
public:
	Bus(std::vector<uint8_t> BIOS, std::vector<uint8_t> cartData, std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<PPU> ppu, std::shared_ptr<Input> input, std::shared_ptr<Scheduler> scheduler);
	~Bus();

	uint8_t read8(uint32_t address, AccessType accessType);
	void write8(uint32_t address, uint8_t value, AccessType accessType);

	uint16_t read16(uint32_t address, AccessType accessType);
	void write16(uint32_t address, uint16_t value, AccessType accessType);

	uint32_t read32(uint32_t address, AccessType accessType);
	void write32(uint32_t address, uint32_t value, AccessType accessType);

	uint32_t fetch32(uint32_t address, AccessType accessType);
	uint16_t fetch16(uint32_t address, AccessType accessType);

	bool getHalted();

//handle IO separately

	uint8_t readIO8(uint32_t address);
	void writeIO8(uint32_t address, uint8_t value);

	uint16_t readIO16(uint32_t address);
	void writeIO16(uint32_t address, uint16_t value);

	uint32_t readIO32(uint32_t address);
	void writeIO32(uint32_t address, uint32_t value);

	static void DMA_VBlankCallback(void* context);
	static void DMA_HBlankCallback(void* context);
	static void DMA_ImmediateCallback(void* context);
	static void DMA_VideoCaptureCallback(void* context);
	static void DMA_AudioFIFOCallback(void* context);

	void tickPrefetcher(uint64_t cycles);
	void invalidatePrefetchBuffer();
private:
	std::shared_ptr<Scheduler> m_scheduler;
	std::shared_ptr<GBAMem> m_mem;
	std::shared_ptr<InterruptManager> m_interruptManager;
	std::shared_ptr<PPU> m_ppu;
	std::shared_ptr<Input> m_input;
	std::shared_ptr<Timer> m_timer;
	std::shared_ptr<APU> m_apu;
	std::shared_ptr<SerialStub> m_serial;

	std::shared_ptr<BackupBase> m_backupMemory;
	BackupType m_backupType = BackupType::None;
	bool backupInitialised = false;	//<--this might be bad, but necessary for EEPROM detection bc we use DMA

	uint32_t romSize = 0;
	bool mirrorROM = false;
	OpenBus m_openBusVals = {};

	DMAChannel m_dmaChannels[4];
	uint16_t WAITCNT = 0;
	uint8_t POSTFLG = 0;
	bool shouldHalt = false;
	bool isFlash = false;

	bool biosLockout = false;
	bool dmaInProgress = false;
	bool dmaNonsequentialAccess = true;

	void attemptSaveAutodetection(std::string_view& romData);

	uint16_t getValue16(uint8_t* arr, int base, int mask);
	void setValue16(uint8_t* arr, int base, int mask, uint16_t val);

	uint32_t getValue32(uint8_t* arr, int base, int mask);
	void setValue32(uint8_t* arr, int base, int mask, uint32_t val);

	void setByteInWord(uint32_t* word, uint8_t byte, int pos);
	void setByteInHalfword(uint16_t* halfword, uint8_t byte, int pos);

	uint8_t DMARegRead(uint32_t address);
	void DMARegWrite(uint32_t address, uint8_t value);
	void checkDMAChannel(int idx);
	void doDMATransfer(int channel);
	void onVBlank();
	void onHBlank();
	void onImmediate();
	void onVideoCapture();
	void onAudioFIFO();

	int waitstateNonsequentialTable[3] = {3,3,3};
	int waitstateSequentialTable[3] = { 1,1,1 };
	int SRAMCycles = 8;
	bool prefetchEnabled = false;

	const int nonseqLUT[4] = { 4,3,2,8 };

	uint32_t m_prefetchHead = 0;
	int prefetchSize = 0;
	int prefetchStart = 0, prefetchEnd = 0;
	bool prefetchInProgress = false;
	bool prefetcherHalted = false;

	uint16_t getPrefetchedValue(uint32_t pc);

	uint64_t prefetchInternalCycles = 0;
	uint64_t prefetchTargetCycles = 0;
	uint32_t prefetchAddress = 0;
	bool prefetchShouldDelay = false;
	bool hack_lastPrefetchGood = false;
};