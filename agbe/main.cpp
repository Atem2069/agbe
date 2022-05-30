#include"Logger.h"
#include"Display.h"

#include<iostream>

int main()
{
	Logger::getInstance()->msg(LoggerSeverity::Info, "Hello world!");

	//have display in main thread here. then, spawn GBA instance on a separate thread running asynchronously.
	//ppu can be polled whenever necessary pretty simply, by having some vfunc to return a framebuffer - uploaded to display
	Display m_display(4);

	while (!m_display.getShouldClose())
	{
		//update texture
		m_display.draw();
	}

	return 0;
}