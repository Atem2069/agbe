#pragma once

#include"Logger.h"
#include"GBAMem.h"
#include"InterruptManager.h"
#include"Scheduler.h"

#include<array>
#include<Windows.h>

struct BG
{
	int priorityBits;
	int bgNumber;
	bool enabled;
	bool affine;
	uint16_t lineBuffer[240];
};

struct BlendAttribute
{
	uint16_t color;
	bool blendB;
	int priority;
};

union SpriteAttribute
{
	uint8_t attr;
	struct
	{
		uint8_t priority : 5;
		bool objWindow : 1;
		bool transparent : 1;
		bool mosaic : 1;
	};
};

struct OAMEntry
{
	uint16_t attr0;
	uint16_t attr1;
	uint16_t attr2;
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
	void registerDMACallbacks(callbackFn HBlank, callbackFn VBlank, callbackFn videoCapture, void*ctx);
	static void onSchedulerEvent(void* context);

	int getVCOUNT();

private:
	bool registered = false;
	std::shared_ptr<GBAMem> m_mem;
	std::shared_ptr<InterruptManager> m_interruptManager;
	std::shared_ptr<Scheduler> m_scheduler;
	uint32_t m_renderBuffer[2][240 * 160];	//currently being rendered
	bool pageIdx = false;
	uint8_t m_bgPriorities[240] = {};	//save bg priority at each pixel
	uint16_t m_spriteLineBuffer[240] = {};
	SpriteAttribute m_spriteAttrBuffer[240] = {};

	int m_spriteCyclesElapsed = 0;		//checks how many cycles have elapsed since sprite rendering started, to enforce the max allowed cycles for sprite pre-rendering

	BG m_backgroundLayers[4];

	PPUState m_state = {};

	uint32_t m_lineCycles = 0;
	bool inVBlank = false;
	bool vblank_setHblankBit = false;
	bool hblank_flagSet = false;

	callbackFn DMAHBlankCallback = nullptr;
	callbackFn DMAVBlankCallback = nullptr;
	callbackFn DMAVideoCaptureCallback = nullptr;
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
	void renderMode5();

	void composeLayers();

	void drawBackground(int bg);
	void drawRotationScalingBackground(int bg);
	void drawSprites();
	void drawAffineSprite(OAMEntry* curSpriteEntry);

	uint16_t extractColorFromTile(uint32_t tileBase, uint32_t xOffset, bool hiColor, bool sprite, uint32_t palette);

	uint16_t blendBrightness(uint16_t col, bool increase);
	uint16_t blendAlpha(uint16_t colA, uint16_t colB);
	uint16_t target2Pixels[240] = {};

	void setVBlankFlag(bool value);
	void setHBlankFlag(bool value);
	void setVCounterFlag(bool value);

	uint32_t col16to32(uint16_t col);

	bool getPointDrawable(int x, int y, int backgroundLayer, bool obj);
	bool getPointBlendable(int x, int y);
	void calcAffineCoords(int32_t& xRef, int32_t& yRef, int16_t dx, int16_t dy);	//<-- put into own function because mosaic can affect *when* these are updated
	void updateAffineRegisters(int bg);												//similar story, however the reference points are actually updated per-scanline

	int affineHorizontalMosaicCounter = 0;
	int affineVerticalMosaicCounter = 0;

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
	uint32_t BG2X = {}, BG2X_latch = {};
	uint32_t BG2Y = {}, BG2Y_latch = {};
	uint32_t BG3X = {}, BG3X_latch = {};
	uint32_t BG3Y = {}, BG3Y_latch = {};
	uint16_t BG2PA = 0x100;
	uint16_t BG2PB = {};
	uint16_t BG2PC = {};
	uint16_t BG2PD = 0x100;
	uint16_t BG3PA = 0x100;
	uint16_t BG3PB = {};
	uint16_t BG3PC = {};
	uint16_t BG3PD = 0x100;
	uint16_t BLDCNT = {};
	uint16_t BLDALPHA = {};
	uint8_t BLDY = {};
	uint16_t MOSAIC = {};
};