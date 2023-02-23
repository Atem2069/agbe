#pragma once

#include"Logger.h"
#include"GBAMem.h"
#include"InterruptManager.h"
#include"Scheduler.h"

#include<array>
#include<Windows.h>

#include<thread>
#include<mutex>
#include<condition_variable>

struct BG
{
	int priorityBits;
	bool masterEnable;
	bool enabled;
	bool affine;
	uint16_t lineBuffer[240];

	bool pendingEnable;	//used to impl. 3 scanline delay
	int scanlinesSinceEnable;
};

struct Window
{
	int16_t x1;
	int16_t x2;
	int16_t y1;
	int16_t y2;
	bool layerDrawable[4];
	bool objDrawable;
	bool blendable;
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

union OAMEntry
{
	uint64_t data;
	struct
	{
		//attribute 0
		unsigned yCoord : 8;
		unsigned rotateScale : 1;
		unsigned disableObj : 1;    //depends on mode. regular sprites use this as 'disable' flag, affine use it as doublesize flag 
		unsigned objMode : 2;
		unsigned mosaic : 1;
		unsigned bitDepth : 1;
		unsigned shape : 2;
		//attribute 1
		unsigned xCoord : 9;
		unsigned unused : 3;		//in rot/scale bits 9-13 of attr1 are actually the parameter selection
		unsigned xFlip : 1;
		unsigned yFlip : 1;
		unsigned size : 2;
		//attribute 2
		unsigned charName : 10;
		unsigned priority : 2;
		unsigned paletteNumber : 4;
	};
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

	void reset();
	void updateDisplayOutput();

	void registerMemory(std::shared_ptr<GBAMem> mem);

	uint8_t readIO(uint32_t address);
	void writeIO(uint32_t address, uint8_t value);

	void registerDMACallbacks(callbackFn HBlank, callbackFn VBlank, callbackFn videoCapture, void*ctx);
	static void onSchedulerEvent(void* context);
	static void onHBlankIRQEvent(void* context);

	int getVCOUNT();
	static uint32_t m_safeDisplayBuffer[240 * 160];
private:
	std::thread renderThread;
	std::mutex mtx;
	std::condition_variable cv;

	bool registered = false;
	std::shared_ptr<GBAMem> m_mem;
	std::shared_ptr<InterruptManager> m_interruptManager;
	std::shared_ptr<Scheduler> m_scheduler;
	uint32_t m_renderBuffer[2][240 * 160];	//currently being rendered
	bool pageIdx = false;
	uint16_t m_spriteLineBuffer[240] = {};
	SpriteAttribute m_spriteAttrBuffer[240] = {};

	int m_spriteCyclesElapsed = 0;		//checks how many cycles have elapsed since sprite rendering started, to enforce the max allowed cycles for sprite pre-rendering

	BG m_backgroundLayers[4];
	Window m_windows[4];

	PPUState m_state = {};

	uint32_t m_lineCycles = 0;
	bool inVBlank = false;
	bool vblank_setHblankBit = false;
	bool hblank_flagSet = false;

	callbackFn DMAHBlankCallback = nullptr;
	callbackFn DMAVBlankCallback = nullptr;
	callbackFn DMAVideoCaptureCallback = nullptr;
	void* callbackContext = nullptr;

	void eventHandler();
	void triggerHBlankIRQ();

	void HDraw();
	void HBlank();
	void VBlank();

	void checkVCOUNTInterrupt();
	bool vcountIRQLine = false;

	void drawScreen();

	void renderMode0();
	void renderMode1();
	void renderMode2();
	void renderMode3();
	void renderMode4();
	void renderMode5();

	void composeLayers();

	void drawBackground(int bg);
	void drawRotationScalingBackground(int bg);
	void drawSprites(bool bitmapMode=false);
	void drawAffineSprite(OAMEntry* curSpriteEntry);

	uint16_t extractColorFromTile(uint32_t tileBase, uint32_t xOffset, bool hiColor, bool sprite, uint32_t palette);

	uint16_t blendBrightness(uint16_t col, bool increase);
	uint16_t blendAlpha(uint16_t colA, uint16_t colB);

	void setVBlankFlag(bool value);
	void setHBlankFlag(bool value);
	void setVCounterFlag(bool value);

	uint32_t col16to32(uint16_t col);

	Window getWindowAttributes(int x, int y);

	void latchBackgroundEnableBits();

	inline void m_calcAffineCoords(bool doMosaic, int32_t& xRef, int32_t& yRef, int16_t dx, int16_t dy)	//<-- put into own function because mosaic can affect *when* these are updated
	{
		if (doMosaic)
		{
			int maxHorizontalMosaic = (MOSAIC & 0xF) + 1;
			affineHorizontalMosaicCounter++;
			if (affineHorizontalMosaicCounter == maxHorizontalMosaic)
			{
				affineHorizontalMosaicCounter = 0;
				xRef += (dx * maxHorizontalMosaic);
				yRef += (dy * maxHorizontalMosaic);
			}
		}
		else
		{
			xRef += dx;
			yRef += dy;
		}
	}

	inline void m_updateAffineRegisters(bool doMosaic, int bg)											//similar story, however the reference points are actually updated per-scanline
	{
		int multiplyAmount = 1;
		if (doMosaic)
		{
			int maxVerticalMosaic = ((MOSAIC >> 4) & 0xF) + 1;
			affineVerticalMosaicCounter++;
			if (affineVerticalMosaicCounter == maxVerticalMosaic)
			{
				affineVerticalMosaicCounter = 0;
				multiplyAmount = maxVerticalMosaic;
			}
			else
				return;
		}
		if (bg == 2)
		{
			int16_t dmx = BG2PB;
			int16_t dmy = BG2PD;

			if ((BG2X_latch >> 27) & 0b1)
				BG2X_latch |= 0xF0000000;
			if ((BG2Y_latch >> 27) & 0b1)
				BG2Y_latch |= 0xF0000000;

			BG2X_latch = (BG2X_latch + (dmx*multiplyAmount)) & 0xFFFFFFF;
			BG2Y_latch = (BG2Y_latch + (dmy*multiplyAmount)) & 0xFFFFFFF;
		}
		if (bg == 3)
		{
			int16_t dmx = BG3PB;
			int16_t dmy = BG3PD;

			if ((BG3X_latch >> 27) & 0b1)
				BG3X_latch |= 0xF0000000;
			if ((BG3Y_latch >> 27) & 0b1)
				BG3Y_latch |= 0xF0000000;

			BG3X_latch = (BG3X_latch + (dmx*multiplyAmount)) & 0xFFFFFFF;
			BG3Y_latch = (BG3Y_latch + (dmy*multiplyAmount)) & 0xFFFFFFF;
		}
	}

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
	uint16_t WININ = {};
	uint16_t WINOUT = {};
	uint32_t BG2X = {}, BG2X_latch = {};
	uint32_t BG2Y = {}, BG2Y_latch = {};
	uint32_t BG3X = {}, BG3X_latch = {};
	uint32_t BG3Y = {}, BG3Y_latch = {};
	bool BG2X_dirty = false, BG2Y_dirty = false, BG3X_dirty = false, BG3Y_dirty = false;	//flags for whether we need to latch new values when new scanline starts
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