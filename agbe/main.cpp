#include"Logger.h"
#include"Display.h"
#include"GBA.h"

#include<iostream>
#include<thread>

void emuWorkerThread();

int main()
{
	Logger::getInstance()->msg(LoggerSeverity::Info, "Hello world!");

	//have display in main thread here. then, spawn GBA instance on a separate thread running asynchronously.
	//ppu can be polled whenever necessary pretty simply, by having some vfunc to return a framebuffer - uploaded to display
	Display m_display(4);

	std::thread m_workerThread(&emuWorkerThread);

	while (!m_display.getShouldClose())
	{
		//update texture
		m_display.draw();
	}

	m_workerThread.join();

	return 0;
}

void emuWorkerThread()
{
	Logger::getInstance()->msg(LoggerSeverity::Info, "Entered worker thread!!");
	
	GBA m_gba;
	m_gba.run();

	Logger::getInstance()->msg(LoggerSeverity::Info, "Exited worker thread!!");
}