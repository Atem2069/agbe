#include"Logger.h"
#include"Display.h"
#include"GBA.h"

#include<iostream>
#include<thread>
#include<filesystem>
#include<Windows.h>

void emuWorkerThread();
void dragDropCallback(GLFWwindow* window, int count, const char** paths);
std::shared_ptr<GBA> m_gba;
std::shared_ptr<InputState> inputState;

FILE* coutStream, *cinStream;

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PSTR lpCmdLine, _In_ int nShowCmd)
{
	AllocConsole();
	freopen_s(&coutStream, "CONOUT$", "w", stdout);
	freopen_s(&cinStream, "CONIN$", "w+", stdin);
	
	std::string path = lpCmdLine;
	if (path.size())
	{
		if (path[0] == '"')
			path = std::string(path.begin() + 1, path.end() - 1);	//i love windows inserting quotes into path :)
		if (std::filesystem::exists(path))							//doublecheck some random crap hasn't been given as an argument
		{
			Config::GBA.shouldReset = true;
			Config::GBA.RomName = path;
		}
	}

	Logger::getInstance()->msg(LoggerSeverity::Info, "Hello world!");
	//have display in main thread here. then, spawn GBA instance on a separate thread running asynchronously.
	//ppu can be polled whenever necessary pretty simply, by having some vfunc to return a framebuffer - uploaded to display
	Display m_display(4);
	m_display.registerDragDropCallback((GLFWdropfun)dragDropCallback);

	inputState = std::make_shared<InputState>();
	std::thread m_workerThread;
	Config::GBA.shouldReset = true;
	while (!m_display.getShouldClose())
	{
		if (Config::GBA.shouldReset)
		{
			if (m_workerThread.joinable())
				m_workerThread.join();
			m_gba = nullptr;
			if (Config::GBA.RomName.length())
			{
				m_gba = std::make_shared<GBA>();
				m_gba->registerInput(inputState);
				m_workerThread = std::thread(&emuWorkerThread);
			}
		}
		else
		{
			//update texture
			void* data = m_gba->getPPUData();
			if (data != nullptr)
				m_display.update(data);
		}
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

	if (m_workerThread.joinable())
	{
		Config::GBA.shouldReset = true;
		m_workerThread.join();
	}
	m_gba = nullptr;

	return 0;
}

void emuWorkerThread()
{
	Logger::getInstance()->msg(LoggerSeverity::Info, "Entered worker thread!!");
	m_gba->run();

	Logger::getInstance()->msg(LoggerSeverity::Info, "Exited worker thread!!");
}

void dragDropCallback(GLFWwindow* window, int count, const char** paths)
{
	if (count > 0)
	{
		Config::GBA.shouldReset = true;
		Config::GBA.RomName = paths[0];
	}
}