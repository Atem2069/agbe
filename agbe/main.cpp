#include"Logger.h"
#include"Display.h"
#include"GBA.h"

#include<iostream>
#include<thread>
#include<filesystem>

void emuWorkerThread();
std::shared_ptr<GBA> m_gba;
std::shared_ptr<InputState> inputState;

int main()
{
	Logger::getInstance()->msg(LoggerSeverity::Info, "Hello world!");
	Config::GBA.exePath = std::filesystem::current_path().string();
	//have display in main thread here. then, spawn GBA instance on a separate thread running asynchronously.
	//ppu can be polled whenever necessary pretty simply, by having some vfunc to return a framebuffer - uploaded to display
	Display m_display(4);

	inputState = std::make_shared<InputState>();
	m_gba = std::make_shared<GBA>();
	std::thread m_workerThread(&emuWorkerThread);

	while (!m_display.getShouldClose())
	{
		//update texture
		void* data = m_gba->getPPUData();
		if(data!=nullptr)
			m_display.update(data);
		m_display.draw();

		//blarg. key input
		inputState->A = m_display.getPressed(GLFW_KEY_X);
		inputState->B = m_display.getPressed(GLFW_KEY_Z);
		inputState->L = m_display.getPressed(GLFW_KEY_A);
		inputState->R = m_display.getPressed(GLFW_KEY_S);
		inputState->Start = m_display.getPressed(GLFW_KEY_ENTER);
		inputState->Select = m_display.getPressed(GLFW_KEY_RIGHT_SHIFT);
		inputState->Up = m_display.getPressed(GLFW_KEY_UP);
		inputState->Down = m_display.getPressed(GLFW_KEY_DOWN);
		inputState->Left = m_display.getPressed(GLFW_KEY_LEFT);
		inputState->Right = m_display.getPressed(GLFW_KEY_RIGHT);

	}

	m_gba->notifyDetach();	//tell gba instance to stop

	m_workerThread.join();

	return 0;
}

void emuWorkerThread()
{
	Logger::getInstance()->msg(LoggerSeverity::Info, "Entered worker thread!!");
	m_gba->registerInput(inputState);
	m_gba->run();

	Logger::getInstance()->msg(LoggerSeverity::Info, "Exited worker thread!!");
}