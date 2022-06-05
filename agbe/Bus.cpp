#include"Bus.h"

Bus::Bus(std::vector<uint8_t> BIOS, std::vector<uint8_t> cartData, std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<PPU> ppu, std::shared_ptr<Input> input)
{
	m_interruptManager = interruptManager;
	m_ppu = ppu;
	m_input = input;

	m_mem = std::make_shared<GBAMem>();
	m_ppu->registerMemory(m_mem);
	if (BIOS.size() != 16385)
	{
		std::cout << BIOS.size() << '\n';
		Logger::getInstance()->msg(LoggerSeverity::Error, "Invalid BIOS ROM size!!");
		return;
	}
	if (cartData.size()-1 > (32 * 1024 * 1024))
	{
		Logger::getInstance()->msg(LoggerSeverity::Error, "ROM file is too big!!");
		return;
	}
	memcpy(m_mem->BIOS, &BIOS[0], BIOS.size());
	memcpy(m_mem->ROM, &cartData[0], cartData.size());	//ROM seems to be mirrored if size <= 16mb. should add later (classic nes might rely on it?)
}

Bus::~Bus()
{

}

void Bus::tick()
{
	//todo: tick other components
	m_ppu->step();
}

uint8_t Bus::read8(uint32_t address, bool doTick)
{
	if (doTick)
		tick();
	uint8_t page = (address >> 24) & 0xF;
	switch (page)
	{
	case 0: case 1:
		if (address >= 0x4000)
		{
			Logger::getInstance()->msg(LoggerSeverity::Error, "Open bus BIOS read");
			return 0;
		}
		return m_mem->BIOS[address & 0x3FFF];
	case 2:
		return m_mem->externalWRAM[address & 0x3FFFF];
	case 3:
		return m_mem->internalWRAM[address & 0x7FFF];
	case 4:
		return readIO8(address);
	case 5:
		return m_mem->paletteRAM[address & 0x3FF];
	case 6:
		address = address & 0x1FFFF;
		if (address >= 0x18000)
			address -= 32768;
		return m_mem->VRAM[address];
	case 7:
		return m_mem->OAM[address & 0x3FF];
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD:	//need to do this better (different waitstates will have different timings)
		return m_mem->ROM[address & 0x01FFFFFF];
	case 0xE:
		Logger::getInstance()->msg(LoggerSeverity::Error, "SRAM not implemented");
		return 0;
	}

	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid read addr={:#x}", address));
	return 0;
}

void Bus::write8(uint32_t address, uint8_t value, bool doTick)
{
	if (doTick)
		tick();
	uint8_t page = (address >> 24) & 0xF;
	switch (page)
	{
	case 0: case 1:
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried to write to BIOS region");
		break;
	case 2:
		m_mem->externalWRAM[address & 0x3FFFF] = value;
		break;
	case 3:
		m_mem->internalWRAM[address & 0x7FFF] = value;
		break;
	case 4:
		writeIO8(address, value);
		break;
	case 5: case 6: case 7:
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried 8-bit write to VRAM - ignoring");
		break;
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD:
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried writing to ROM - ignoring");
		break;
	case 0xE:
		Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented SRAM write");
		break;
	default:
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid write addr={:#x}", address));
		break;
	}
}

uint16_t Bus::read16(uint32_t address, bool doTick)
{
	if (doTick)
		tick();
	address &= 0xFFFFFFFE;
	uint8_t page = (address >> 24) & 0xF;

	switch (page)
	{
	case 0: case 1:
		if (address > 0x3FFF)
		{
			Logger::getInstance()->msg(LoggerSeverity::Error, "Out of bounds BIOS read");
			return 0;
		}
		return getValue16(m_mem->BIOS, address & 0x3FFF);
	case 2:
		return getValue16(m_mem->externalWRAM, address & 0x3FFFF);
	case 3:
		return getValue16(m_mem->internalWRAM, address & 0x7FFF);
	case 4:
		return readIO16(address);
	case 5:
		return getValue16(m_mem->paletteRAM, address & 0x3FF);
	case 6:
		address = address & 0x1FFFF;
		if (address >= 0x18000)
			address -= 32768;
		return getValue16(m_mem->VRAM, address);
	case 7:
		return getValue16(m_mem->OAM, address & 0x3FF);
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD:
		return getValue16(m_mem->ROM, address & 0x01FFFFFF);
	case 0xE:
		Logger::getInstance()->msg(LoggerSeverity::Error, "Invalid 16-bit SRAM read");
		return 0;
	}

	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid read addr={:#x}", address));
	return 0;
}

void Bus::write16(uint32_t address, uint16_t value, bool doTick)
{
	if (doTick)
		tick();
	address &= 0xFFFFFFFE;
	uint8_t page = (address >> 24) & 0xF;
	switch (page)
	{
	case 0: case 1:
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried to write to BIOS!!");
		break;
	case 2:
		setValue16(m_mem->externalWRAM, address & 0x3FFFF, value);
		break;
	case 3:
		setValue16(m_mem->internalWRAM, address & 0x7FFF, value);
		break;
	case 4:
		writeIO16(address, value);
		break;
	case 5:
		setValue16(m_mem->paletteRAM, address & 0x3FF, value);
		break;
	case 6:
		address = address & 0x1FFFF;
		if (address >= 0x18000)
			address -= 32768;
		setValue16(m_mem->VRAM, address, value);
		break;
	case 7:
		setValue16(m_mem->OAM, address & 0x3FF, value);
		break;
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD: case 0xE:
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried to write to cartridge space!!!");
		break;
	default:
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid write addr={:#x}", address));
		break;
	}
}

uint32_t Bus::read32(uint32_t address, bool doTick)
{
	if (doTick)
		tick();
	address &= 0xFFFFFFFC;
	uint8_t page = (address >> 24) & 0xF;

	switch (page)
	{
	case 0: case 1:
		if (address > 0x3FFF)
		{
			Logger::getInstance()->msg(LoggerSeverity::Error, "Out of bounds BIOS read");
			return 0;
		}
		return getValue32(m_mem->BIOS, address & 0x3FFF);
	case 2:
		return getValue32(m_mem->externalWRAM, address & 0x3FFFF);
	case 3:
		return getValue32(m_mem->internalWRAM, address & 0x7FFF);
	case 4:
		return readIO32(address);
	case 5:
		return getValue32(m_mem->paletteRAM, address & 0x3FF);
	case 6:
		address = address & 0x1FFFF;
		if (address >= 0x18000)
			address -= 32768;
		return getValue32(m_mem->VRAM, address);
	case 7:
		return getValue32(m_mem->OAM, address & 0x3FF);
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD:
		return getValue32(m_mem->ROM, address & 0x01FFFFFF);
	case 0xE:
		Logger::getInstance()->msg(LoggerSeverity::Error, "Invalid 32-bit SRAM read");
		return 0;
	}

	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid read addr={:#x}", address));
	return 0;
}

void Bus::write32(uint32_t address, uint32_t value, bool doTick)
{
	if (doTick)
		tick();
	address &= 0xFFFFFFFC;
	uint8_t page = (address >> 24) & 0xF;
	switch (page)
	{
	case 0: case 1:
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried to write to BIOS!!");
		break;
	case 2:
		setValue32(m_mem->externalWRAM, address & 0x3FFFF, value);
		break;
	case 3:
		setValue32(m_mem->internalWRAM, address & 0x7FFF, value);
		break;
	case 4:
		writeIO32(address, value);
		break;
	case 5:
		setValue32(m_mem->paletteRAM, address & 0x3FF, value);
		break;
	case 6:
		address = address & 0x1FFFF;
		if (address >= 0x18000)
			address -= 32768;
		setValue32(m_mem->VRAM, address, value);
		break;
	case 7:
		setValue32(m_mem->OAM, address & 0x3FF, value);
		break;
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD: case 0xE:
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried to write to cartridge space!!!");
		break;
	default:
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid write addr={:#x}", address));
		break;
	}
}


//Probably handle reading a single IO byte in 'readIO8'
//And then sort out 16/32 bit r/w using the above
uint8_t Bus::readIO8(uint32_t address)
{
	if (address <= 0x04000056)
		return m_ppu->readIO(address);
	if (address >= 0x04000130 && address <= 0x04000133)
		return m_input->readIORegister(address);
	switch (address)
	{
	case 0x04000200:case 0x04000201: case 0x04000202:  case 0x04000203: case 0x04000208: case 0x04000209: case 0x0400020A: case 0x0400020B:
		return m_interruptManager->readIO(address);
	}
	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unimplemented IO read addr={:#x}", address));
	return 0;
}

void Bus::writeIO8(uint32_t address, uint8_t value)
{
	if (address <= 0x04000056)
	{
		m_ppu->writeIO(address,value);
		return;
	}
	if (address >= 0x04000130 && address <= 0x04000133)
	{
		m_input->writeIORegister(address, value);
		return;
	}
	switch (address)
	{
	case 0x04000200:case 0x04000201: case 0x04000202:  case 0x04000203: case 0x04000208: case 0x04000209: case 0x0400020A: case 0x0400020B:
		m_interruptManager->writeIO(address,value);
		return;
	}
	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unimplemented IO write addr={:#x}", address));
}

uint16_t Bus::readIO16(uint32_t address)
{
	uint8_t lower = readIO8(address);
	uint8_t upper = readIO8(address + 1);
	return (uint16_t)(upper << 8) | lower;
}

void Bus::writeIO16(uint32_t address, uint16_t value)
{
	writeIO8(address, value & 0xFF);
	writeIO8(address + 1, ((value >> 8) & 0xFF));
}

uint32_t Bus::readIO32(uint32_t address)
{
	uint16_t lower = readIO16(address);		//nicer than readIO8 4 times
	uint16_t upper = readIO16(address + 2);
	return (uint32_t)(upper << 16) | lower;
	return 0;
}

void Bus::writeIO32(uint32_t address, uint32_t value)
{
	writeIO16(address, value & 0xFFFF);
	writeIO16(address + 2, ((value >> 16) & 0xFFFF));
}


//Handles reading/writing larger than byte sized values (the addresses should already be aligned so no issues there)
//This is SOLELY for memory - IO is handled differently bc it's not treated as a flat mem space
uint16_t Bus::getValue16(uint8_t* arr, int base)
{
	return (uint16_t)arr[base] | ((arr[base + 1]) << 8);
}

void Bus::setValue16(uint8_t* arr, int base, uint16_t val)
{
	arr[base] = val & 0xFF;
	arr[base + 1] = ((val >> 8) & 0xFF);
}

uint32_t Bus::getValue32(uint8_t* arr, int base)
{
	return (uint32_t)arr[base] | ((arr[base + 1]) << 8) | ((arr[base + 2]) << 16) | ((arr[base + 3]) << 24);
}

void Bus::setValue32(uint8_t* arr, int base, uint32_t val)
{
	arr[base] = val & 0xFF;
	arr[base + 1] = ((val >> 8) & 0xFF);
	arr[base + 2] = ((val >> 16) & 0xFF);
	arr[base + 3] = ((val >> 24) & 0xFF);
}