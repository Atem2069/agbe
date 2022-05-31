#pragma once

#include<iostream>

//help facilitate easy sharing of memory between exclusive components
struct GBAMem
{
	uint8_t BIOS[16384];
	uint8_t externalWRAM[256 * 1024];
	uint8_t internalWRAM[32 * 1024];
	uint8_t paletteRAM[1024];
	uint8_t VRAM[96 * 1024];
	uint8_t OAM[1024];
	uint8_t ROM[32 * 1024 * 1024];
};