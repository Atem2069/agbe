#pragma once

#include"Logger.h"
#include"GBAMem.h"
#include"InterruptManager.h"
#include"Scheduler.h"

#include<array>
#include<Windows.h>

//this isn't a great approach to bg priority !!
struct BGSortItem
{
	int priorityBits;
	int bgNumber;
	bool enabled;
	bool affine;
	static bool sortDescending(const BGSortItem& lhs, const BGSortItem& rhs)	//sort by decreasing priority
	{
		return lhs.priorityBits > rhs.priorityBits;
	}
};

enum class PPUState
{
	HDraw,
	HBlank,
	VBlank,
	VBlankHBlank
};

class PPU
{
public:
	PPU(std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<Scheduler> scheduler);
	~PPU();

	void registerMemory(std::shared_ptr<GBAMem> mem);

	uint32_t* getDisplayBuffer();

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);

	bool getShouldSync();
	void registerDMACallbacks(callbackFn HBlank, callbackFn VBlank, void*ctx);
	static void onSchedulerEvent(void* context);

private:
	bool registered = false;
	std::shared_ptr<GBAMem> m_mem;
	std::shared_ptr<InterruptManager> m_interruptManager;
	std::shared_ptr<Scheduler> m_scheduler;
	uint32_t m_renderBuffer[2][240 * 160];	//currently being rendered
	bool pageIdx = false;
	//uint32_t m_displayBuffer[240 * 160]; //buffer the display gets
	uint8_t m_bgPriorities[240] = {};	//save bg priority at each pixel
	uint8_t m_spritePriorities[240] = {};
	uint32_t m_spriteLineBuffer[240] = {};
	uint8_t m_objWindowMask[240] = {};

	PPUState m_state = {};

	uint32_t m_lineCycles = 0;
	bool inVBlank = false;
	bool vblank_setHblankBit = false;
	bool hblank_flagSet = false;

	callbackFn DMAHBlankCallback = nullptr;
	callbackFn DMAVBlankCallback = nullptr;
	void* callbackContext = nullptr;
	bool shouldSyncVideo = false;

	void eventHandler();
	uint64_t expectedNextTimeStamp = 0;

	void HDraw();
	void HBlank();
	void VBlank();

	void renderMode0();
	void renderMode1();
	void renderMode2();
	void renderMode3();
	void renderMode4();

	void drawBackground(int bg);
	void drawRotationScalingBackground(int bg);
	void drawSprites();
	uint16_t blendBrightness(uint16_t col, bool increase);
	uint16_t blendAlpha(uint16_t colA, uint16_t colB);
	uint16_t target2Pixels[240] = {};

	void setVBlankFlag(bool value);
	void setHBlankFlag(bool value);
	void setVCounterFlag(bool value);

	uint32_t col16to32(uint16_t col);

	bool getPointDrawable(int x, int y, int backgroundLayer, bool obj);
	bool getPointBlendable(int x, int y);

	uint16_t DISPCNT = {};
	uint16_t DISPSTAT = {};
	uint16_t VCOUNT = {};
	uint16_t BG0CNT = {};
	uint16_t BG1CNT = {};
	uint16_t BG2CNT = {};
	uint16_t BG3CNT = {};
	uint16_t BG0HOFS = {};
	uint16_t BG0VOFS = {};
	uint16_t BG1HOFS = {};
	uint16_t BG1VOFS = {};
	uint16_t BG2HOFS = {};
	uint16_t BG2VOFS = {};
	uint16_t BG3HOFS = {};
	uint16_t BG3VOFS = {};
	uint16_t WIN0H = {};
	uint16_t WIN1H = {};
	uint16_t WIN0V = {};
	uint16_t WIN1V = {};
	uint16_t WININ = {};
	uint16_t WINOUT = {};
	uint32_t BG2X = {};
	uint32_t BG2Y = {};
	uint32_t BG3X = {};
	uint32_t BG3Y = {};
	uint16_t BG2PA = {};
	uint16_t BG2PB = {};
	uint16_t BG2PC = {};
	uint16_t BG2PD = {};
	uint16_t BG3PA = {};
	uint16_t BG3PB = {};
	uint16_t BG3PC = {};
	uint16_t BG3PD = {};
	uint16_t BLDCNT = {};
	uint16_t BLDALPHA = {};
	uint8_t BLDY = {};
};