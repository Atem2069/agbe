#pragma once

#include"Logger.h"
#include"Bus.h"
#include"ARM7TDMI.h"
#include"PPU.h"
#include"Input.h"
#include"InterruptManager.h"
#include"Config.h"
#include"Scheduler.h"

#include<Windows.h>
#include<mutex>
#include<chrono>

class GBA
{
public:
	GBA();
	~GBA();

	void run();
	void notifyDetach();

	void* getPPUData();
	void registerInput(std::shared_ptr<InputState> inp);
	static void onEvent(void* context);
private:
	std::shared_ptr<Bus> m_bus;
	std::shared_ptr<InterruptManager> m_interruptManager;
	std::shared_ptr<ARM7TDMI> m_cpu;
	std::shared_ptr<PPU> m_ppu;
	std::shared_ptr<Input> m_input;
	std::shared_ptr<Scheduler> m_scheduler;
	std::shared_ptr<InputState> m_inp;

	bool m_shouldStop = false;
	std::chrono::steady_clock::time_point m_lastTime;
	uint64_t expectedNextFrame = 0;
	void frameEventHandler();

	void m_destroy();
	void m_initialise();

	bool m_initialised = false;

	std::vector<uint8_t> readFile(const char* name);
	uint32_t safe_dispBuffer[240 * 160] = {};
};