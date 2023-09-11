#pragma once

#include<iostream>

struct SystemConfig
{
	std::string exePath;
	std::string RomName;
	bool shouldReset;
	bool disableVideoSync;
	double fps = 0;
};

class Config
{
public:
	static SystemConfig GBA;
};