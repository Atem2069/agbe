#pragma once

#include"Logger.h"

#include<iostream>
#include<format>
#include<glad/glad.h>
#include<GLFW/glfw3.h>

//Simple GL display backend. Takes in pixel data from emu and renders as a textured quad

class Display
{
public:
	Display(int scaleFactor);
	~Display();

	bool getShouldClose();
	void draw();
	void update(void* newData);	//unsafe but size is known :)
private:
	GLFWwindow* m_window;
};