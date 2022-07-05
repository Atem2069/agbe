#include"Bus.h"

Bus::Bus(std::vector<uint8_t> BIOS, std::vector<uint8_t> cartData, std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<PPU> ppu, std::shared_ptr<Input> input, std::shared_ptr<Scheduler> scheduler)
{
	busCtx = (void*)this;
	m_scheduler = scheduler;
	m_interruptManager = interruptManager;
	m_ppu = ppu;
	m_input = input;

	m_mem = std::make_shared<GBAMem>();
	m_timer = std::make_shared<Timer>(m_interruptManager,m_scheduler);
	m_apu = std::make_shared<APU>(m_scheduler);
	m_timer->registerAPUCallbacks((callbackFn)&APU::timer0Callback, (callbackFn)&APU::timer1Callback, (void*)m_apu.get());
	m_apu->registerDMACallback((callbackFn)&Bus::DMA_AudioFIFOCallback, busCtx);
	m_serial = std::make_shared<SerialStub>(m_scheduler, m_interruptManager);
	m_ppu->registerMemory(m_mem);
	m_ppu->registerDMACallbacks(&Bus::DMA_HBlankCallback, &Bus::DMA_VBlankCallback, &Bus::DMA_VideoCaptureCallback, (void*)this);
	if (BIOS.size() != 16384)
	{
		std::cout << BIOS.size() << '\n';
		Logger::getInstance()->msg(LoggerSeverity::Error, "Invalid BIOS ROM size!!");
		return;
	}
	if (cartData.size() > (32 * 1024 * 1024))
	{
		Logger::getInstance()->msg(LoggerSeverity::Error, "ROM file is too big!!");
		return;
	}
	romSize = cartData.size();
	for (int i = 0; i < 4; i++)	//clear dma channel registers
		m_dmaChannels[i] = {};
	memcpy(m_mem->BIOS, &BIOS[0], BIOS.size());
	memcpy(m_mem->ROM, &cartData[0], cartData.size());	//ROM seems to be mirrored if size <= 16mb. should add later (classic nes might rely on it?)


	auto romAsString = std::string_view(reinterpret_cast<const char*>(m_mem->ROM), 32 * 1024 * 1024);
	attemptSaveAutodetection(romAsString);

}

Bus::~Bus()
{
	m_mem.reset();
	m_timer.reset();
}

void Bus::attemptSaveAutodetection(std::string_view& romData)
{
	//this doesn't seem to be perfect. some games have strings for multiple backup types bc they're evil :(
	if ((romData.find("FLASH512") != std::string_view::npos) || (romData.find("FLASH_V") != std::string_view::npos))
	{
		backupInitialised = true;
		Logger::getInstance()->msg(LoggerSeverity::Info, "Init 512Kbit flash memory!!");
		m_backupType = BackupType::FLASH512K;
		m_backupMemory = std::make_shared<Flash>(m_backupType);
	}
	else if (romData.find("FLASH1M") != std::string::npos)
	{
		backupInitialised = true;
		Logger::getInstance()->msg(LoggerSeverity::Info, "Init 1Mbit flash memory!!");
		m_backupType = BackupType::FLASH1M;
		m_backupMemory = std::make_shared<Flash>(m_backupType);
	}
	else if (romData.find("SRAM") != std::string::npos)
	{
		backupInitialised = true;
		Logger::getInstance()->msg(LoggerSeverity::Info, "Init SRAM backup memory!!");
		m_backupType = BackupType::SRAM;
		m_backupMemory = std::make_shared<SRAM>(m_backupType);
	}

	if (!backupInitialised)
		Logger::getInstance()->msg(LoggerSeverity::Warn, "Failed to auto-detect savetype. The ROM may be using EEPROM or masking its savetype!");
}

uint8_t Bus::read8(uint32_t address, AccessType accessType)
{
	int cartCycles = 0;
	uint8_t page = (address >> 24) & 0xFF;
	switch (page)
	{
	case 0: case 1:
		tickPrefetcher(1);
		if ((address >= 0x4000) || biosLockout)
		{
			if (address <= 0x3FFF)
				return m_openBusVals.bios;	//todo: account for the value being rotated properly
			return m_openBusVals.mem;
		}
		return m_mem->BIOS[address & 0x3FFF];
	case 2:
		m_scheduler->addCycles(2);
		tickPrefetcher(3);
		return m_mem->externalWRAM[address & 0x3FFFF];
	case 3:
		tickPrefetcher(1);
		return m_mem->internalWRAM[address & 0x7FFF];
	case 4:
		tickPrefetcher(1);
		return readIO8(address);
	case 5:
		tickPrefetcher(1);
		return m_mem->paletteRAM[address & 0x3FF];
	case 6:
		tickPrefetcher(1);
		address = address & 0x1FFFF;
		if (address >= 0x18000)
			address -= 32768;
		return m_mem->VRAM[address];
	case 7:
		tickPrefetcher(1);
		return m_mem->OAM[address & 0x3FF];
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD:	//need to do this better (different waitstates will have different timings)
		cartCycles = ((accessType==AccessType::Sequential) && ((address & 0x1FF) != 0)) ? waitstateSequentialTable[((page - 8) >> 1)] : waitstateNonsequentialTable[((page - 8) >> 1)];
		m_scheduler->addCycles(cartCycles);
		if (prefetchInProgress && prefetchShouldDelay)
			m_scheduler->addCycles(1);
		prefetchShouldDelay = false;
		invalidatePrefetchBuffer();
		if ((address & 0x01FFFFFF) >= romSize)
			return (address / 2) & 0xFFFF;
		return m_mem->ROM[address & 0x01FFFFFF];
	case 0xE: case 0xF:
		m_scheduler->addCycles(SRAMCycles);	//hm.
		if (prefetchInProgress && prefetchShouldDelay)
			m_scheduler->addCycles(1);
		prefetchShouldDelay = false;
		invalidatePrefetchBuffer();
		if (m_backupType == BackupType::FLASH1M || m_backupType == BackupType::FLASH512K || m_backupType == BackupType::SRAM)
			return m_backupMemory->read(address);
	}

	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid read addr={:#x}", address));
	tickPrefetcher(1);
	return m_openBusVals.mem;
}

void Bus::write8(uint32_t address, uint8_t value, AccessType accessType)
{
	int cartCycles = 0;
	uint8_t page = (address >> 24) & 0xFF;
	switch (page)
	{
	case 0: case 1:
		tickPrefetcher(1);
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried to write to BIOS region");
		break;
	case 2:
		m_scheduler->addCycles(2);
		tickPrefetcher(3);
		m_mem->externalWRAM[address & 0x3FFFF] = value;
		break;
	case 3:
		tickPrefetcher(1);
		m_mem->internalWRAM[address & 0x7FFF] = value;
		break;
	case 4:
		tickPrefetcher(1);
		writeIO8(address, value);
		break;
	case 5:
		tickPrefetcher(1);
		m_mem->paletteRAM[address & 0x3FF] = value;
		m_mem->paletteRAM[(address + 1) & 0x3FF] = value;
		break;
	case 6:
		tickPrefetcher(1);
		address = address & 0x1FFFF;
		if (address >= 0x18000)
			address -= 32768;
		m_mem->VRAM[address]=value;
		m_mem->VRAM[address + 1] = value;
		break;
	case 7:
		tickPrefetcher(1);
		Logger::getInstance()->msg(LoggerSeverity::Error, "Ignore obj write");
		break;
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD:
		cartCycles = ((accessType==AccessType::Sequential) && ((address & 0x1FF) != 0)) ? waitstateSequentialTable[((page - 8) >> 1)] : waitstateNonsequentialTable[((page - 8) >> 1)];
		m_scheduler->addCycles(cartCycles);
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried writing to ROM - ignoring");
		if (prefetchInProgress && prefetchShouldDelay)
			m_scheduler->addCycles(1);
		prefetchShouldDelay = false;
		invalidatePrefetchBuffer();
		break;
	case 0xE: case 0xF:
		m_scheduler->addCycles(SRAMCycles);
		if (m_backupType == BackupType::FLASH1M || m_backupType == BackupType::FLASH512K || m_backupType == BackupType::SRAM)
			m_backupMemory->write(address, value);
		break;
	default:
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid write addr={:#x}", address));
		tickPrefetcher(1);
		break;
	}
}

uint16_t Bus::read16(uint32_t address, AccessType accessType)
{
	int cartCycles = 0;
	uint32_t originalAddress = address;
	address &= 0xFFFFFFFE;
	uint8_t page = (address >> 24) & 0xFF;
	switch (page)
	{
	case 0: case 1:
		tickPrefetcher(1);
		if ((address > 0x3FFF) || biosLockout)
		{
			if (address <= 0x3FFF)
				return m_openBusVals.bios;	//todo: account for the value being rotated properly
			Logger::getInstance()->msg(LoggerSeverity::Error, "Out of bounds BIOS read");
			return m_openBusVals.mem;
		}
		return getValue16(m_mem->BIOS, address & 0x3FFF, 0x3FFF);
	case 2:
		m_scheduler->addCycles(2);
		tickPrefetcher(3);
		return getValue16(m_mem->externalWRAM, address & 0x3FFFF, 0x3FFFF);
	case 3:
		tickPrefetcher(1);
		return getValue16(m_mem->internalWRAM, address & 0x7FFF, 0x7FFF);
	case 4:
		tickPrefetcher(1);
		return readIO16(address);
	case 5:
		tickPrefetcher(1);
		return getValue16(m_mem->paletteRAM, address & 0x3FF,0x3FF);
	case 6:
		tickPrefetcher(1);
		address = address & 0x1FFFF;
		if (address >= 0x18000)
			address -= 32768;
		return getValue16(m_mem->VRAM, address,0xFFFFFFFF);
	case 7:
		tickPrefetcher(1);
		return getValue16(m_mem->OAM, address & 0x3FF,0x3FF);
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD:
		dmaNonsequentialAccess = false;
		cartCycles = ((accessType==AccessType::Sequential) && ((address & 0x1FF) != 0)) ? waitstateSequentialTable[((page - 8) >> 1)] : waitstateNonsequentialTable[((page - 8) >> 1)];
		if (accessType != AccessType::Prefetch)
		{
			if (prefetchInProgress && prefetchShouldDelay)
				m_scheduler->addCycles(1);
			prefetchShouldDelay = false;
			invalidatePrefetchBuffer();
			m_scheduler->addCycles(cartCycles);
		}
		if (page==0xD)
		{
			if(m_backupType == BackupType::EEPROM4K || m_backupType == BackupType::EEPROM64K)
				return m_backupMemory->read(address);
		}
		if ((address & 0x01FFFFFF) >= romSize)
			return (address / 2) & 0xFFFF;
		return getValue16(m_mem->ROM, address & 0x01FFFFFF,0xFFFFFFFF);
	case 0xE: case 0xF:
		m_scheduler->addCycles(SRAMCycles);
		if (m_backupType == BackupType::SRAM)
			return m_backupMemory->read(originalAddress) * 0x0101;
	}

	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid read addr={:#x}", address));
	tickPrefetcher(1);
	return m_openBusVals.mem;
}

void Bus::write16(uint32_t address, uint16_t value, AccessType accessType)
{
	int cartCycles = 0;
	uint32_t originalAddress = address;
	address &= 0xFFFFFFFE;
	uint8_t page = (address >> 24) & 0xFF;
	switch (page)
	{
	case 0: case 1:
		tickPrefetcher(1);
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried to write to BIOS!!");
		break;
	case 2:
		m_scheduler->addCycles(2);
		tickPrefetcher(3);
		setValue16(m_mem->externalWRAM, address & 0x3FFFF, 0x3FFFF, value);
		break;
	case 3:
		tickPrefetcher(1);
		setValue16(m_mem->internalWRAM, address & 0x7FFF, 0x7FFF, value);
		break;
	case 4:
		tickPrefetcher(1);
		writeIO16(address, value);
		break;
	case 5:
		tickPrefetcher(1);
		setValue16(m_mem->paletteRAM, address & 0x3FF, 0x3FF, value);
		break;
	case 6:
		tickPrefetcher(1);
		address = address & 0x1FFFF;
		if (address >= 0x18000)
			address -= 32768;
		setValue16(m_mem->VRAM, address, 0xFFFFFFFF, value);
		break;
	case 7:
		tickPrefetcher(1);
		setValue16(m_mem->OAM, address & 0x3FF, 0x3FF, value);
		break;
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD:
		dmaNonsequentialAccess = false;
		cartCycles = ((accessType==AccessType::Sequential) && ((address & 0x1FF) != 0)) ? waitstateSequentialTable[((page - 8) >> 1)] : waitstateNonsequentialTable[((page - 8) >> 1)];
		m_scheduler->addCycles(cartCycles);
		if (prefetchInProgress && prefetchShouldDelay)
			m_scheduler->addCycles(1);
		prefetchShouldDelay = false;
		invalidatePrefetchBuffer();
		if (page==0xD && (m_backupType == BackupType::EEPROM4K || m_backupType == BackupType::EEPROM64K))
		{
			m_backupMemory->write(address, value);
			break;
		}
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Tried to write to cartridge space!!! addr={:#x}",address));
		break;
	case 0xE: case 0xF:
		m_scheduler->addCycles(SRAMCycles);
		if (m_backupType == BackupType::SRAM)
		{
			value = (std::rotr(value, (originalAddress * 8))) & 0xFF;
			m_backupMemory->write(originalAddress & 0xFFFF, value);
		}
		break;
	default:
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid write addr={:#x}", address));
		tickPrefetcher(1);
		break;
	}
}

uint32_t Bus::read32(uint32_t address, AccessType accessType)
{
	int cartCycles = 0;
	uint32_t originalAddress = address;
	address &= 0xFFFFFFFC;
	uint8_t page = (address >> 24) & 0xFF;
	switch (page)
	{
	case 0: case 1:
		tickPrefetcher(1);
		if ((address > 0x3FFF) || biosLockout)
		{
			if (address <= 0x3FFF)
				return m_openBusVals.bios;
			return m_openBusVals.mem;
		}
		m_openBusVals.bios = getValue32(m_mem->BIOS, address & 0x3FFF, 0x3FFF);
		return m_openBusVals.bios;
	case 2:
		m_scheduler->addCycles(5);	//5 bc first access is 2 waitstates, then another access happens which is 1S + 2 waitstates
		tickPrefetcher(6);
		return getValue32(m_mem->externalWRAM, address & 0x3FFFF,0x3FFFF);
	case 3:
		tickPrefetcher(1);
		return getValue32(m_mem->internalWRAM, address & 0x7FFF,0x7FFF);
	case 4:
		tickPrefetcher(1);
		return readIO32(address);
	case 5:
		m_scheduler->addCycles(1);
		tickPrefetcher(2);
		return getValue32(m_mem->paletteRAM, address & 0x3FF,0x3FF);
	case 6:
		m_scheduler->addCycles(1);
		tickPrefetcher(2);
		address = address & 0x1FFFF;
		if (address >= 0x18000)
			address -= 32768;
		return getValue32(m_mem->VRAM, address,0xFFFFFFFF);
	case 7:
		tickPrefetcher(1);
		return getValue32(m_mem->OAM, address & 0x3FF,0x3FF);
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD:
		dmaNonsequentialAccess = false;
		cartCycles = ((accessType == AccessType::Sequential) && ((address & 0x1FF) != 0)) ? waitstateSequentialTable[((page - 8) >> 1)] : waitstateNonsequentialTable[((page - 8) >> 1)];
		cartCycles += waitstateSequentialTable[((page - 8) >> 1)];
		m_scheduler->addCycles(cartCycles + 1);	//first access is either nonseq/seq. second is *always* seq
		if (accessType != AccessType::Prefetch)
		{
			if (prefetchShouldDelay && prefetchInProgress)
			{
				tickPrefetcher(1);
				m_scheduler->addCycles(1);
			}
			prefetchShouldDelay = false;
			invalidatePrefetchBuffer();
		}
		if ((address & 0x01FFFFFF) >= romSize)
			return ((address / 2) & 0xFFFF) | (((address + 2) / 2) & 0xFFFF) << 16;
		return getValue32(m_mem->ROM, address & 0x01FFFFFF, 0xFFFFFFFF);
	case 0xE: case 0xF:
		m_scheduler->addCycles(SRAMCycles);
		if (m_backupType == BackupType::SRAM)
			return m_backupMemory->read(originalAddress) * 0x01010101;
	}

	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid read addr={:#x}", address));
	tickPrefetcher(1);
	return m_openBusVals.mem;
}

void Bus::write32(uint32_t address, uint32_t value, AccessType accessType)
{
	int cartCycles = 0;
	uint32_t originalAddress = address;
	address &= 0xFFFFFFFC;
	uint8_t page = (address >> 24) & 0xFF;
	switch (page)
	{
	case 0: case 1:
		tickPrefetcher(1);
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried to write to BIOS!!");
		break;
	case 2:
		m_scheduler->addCycles(5);
		tickPrefetcher(6);
		setValue32(m_mem->externalWRAM, address & 0x3FFFF, 0x3FFFF, value);
		break;
	case 3:
		tickPrefetcher(1);
		setValue32(m_mem->internalWRAM, address & 0x7FFF, 0x7FFF, value);
		break;
	case 4:
		tickPrefetcher(1);
		writeIO32(address, value);
		break;
	case 5:
		m_scheduler->addCycles(1);
		tickPrefetcher(1);
		setValue32(m_mem->paletteRAM, address & 0x3FF, 0x3FF, value);
		break;
	case 6:
		m_scheduler->addCycles(1);
		tickPrefetcher(1);
		address = address & 0x1FFFF;
		if (address >= 0x18000)
			address -= 32768;
		setValue32(m_mem->VRAM, address, 0xFFFFFFFF, value);
		break;
	case 7:
		tickPrefetcher(1);
		setValue32(m_mem->OAM, address & 0x3FF, 0x3FF, value);
		break;
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD:
		dmaNonsequentialAccess = false;
		cartCycles = ((accessType==AccessType::Sequential) && ((address & 0x1FF) != 0)) ? waitstateSequentialTable[((page - 8) >> 1)] : waitstateNonsequentialTable[((page - 8) >> 1)];
		cartCycles += waitstateSequentialTable[((page - 8) >> 1)];
		m_scheduler->addCycles(cartCycles + 1);	//same setup as for read32
		if (prefetchInProgress && prefetchShouldDelay)
		{
			tickPrefetcher(1);
			m_scheduler->addCycles(1);
		}
		prefetchShouldDelay = false;
		invalidatePrefetchBuffer();
		break;
	case 0xE: case 0xF:
		m_scheduler->addCycles(SRAMCycles);
		if (m_backupType == BackupType::SRAM)
			m_backupMemory->write(originalAddress, (std::rotr(value, originalAddress * 8) & 0xFF));
		break;
	default:
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Out of bounds/invalid write addr={:#x}", address));
		tickPrefetcher(1);
		break;
	}
}

uint32_t Bus::fetch32(uint32_t address, AccessType accessType)
{
	biosLockout = false;
	if (address < 0x08000000 || address > 0x0DFFFFFF)
		invalidatePrefetchBuffer();
	uint32_t val = 0;
	if (prefetchEnabled && address >= 0x08000000 && address <= 0x0DFFFFFF)
	{
		uint16_t valLow = fetch16(address, accessType);
		uint16_t valHigh = fetch16(address + 2, accessType);
		val = ((valHigh << 16) | valLow);
		m_openBusVals.mem = val;
		if(!hack_lastPrefetchGood)
			m_scheduler->addCycles(1);		//extra S cycle inserted only when prefetch fails
	}
	else
		val = read32(address,accessType);
	if(address>0x3FFF)
		biosLockout = true;
	m_openBusVals.mem = val;
	return val;
}

uint16_t Bus::fetch16(uint32_t address, AccessType accessType)
{
	biosLockout = false;
	if (address < 0x08000000 || address > 0x0DFFFFFF)
		invalidatePrefetchBuffer();
	uint16_t val = 0;
	if (prefetchEnabled && address >= 0x08000000 && address <= 0x0DFFFFFF)	//nice, we can just read the prefetch buffer
		val = getPrefetchedValue(address);
	else
		val = read16(address, accessType);
	if(address>0x3FFF)
		biosLockout = true;
	m_openBusVals.mem = val;
	if ((address >> 1) & 0b1)
		m_openBusVals.mem = std::rotr(m_openBusVals.mem, 8);	//hmm, if misaligned word address then rotate the fetched val?
	return val;
}

uint16_t Bus::getPrefetchedValue(uint32_t pc)
{
	hack_lastPrefetchGood = false;
	uint16_t val = 0;
	if (prefetchInProgress)
	{
		if (prefetchSize > 0)	//if value in prefetch buffer, then just get it
		{
			val = m_prefetchBuffer[prefetchStart].value;
			prefetchStart = (prefetchStart + 1) & 7;
			prefetchSize--;
			hack_lastPrefetchGood = true;
		}
		else					//otherwise we'll wait for the prefetch buffer to get it, then reset the buffer (but keep burst going)
		{
			uint8_t page = (pc >> 24) & 0xFF;
			uint64_t waitstates = waitstateSequentialTable[((page - 8) >> 1)];
			m_scheduler->addCycles(waitstates - prefetchInternalCycles);
			invalidatePrefetchBuffer();
			prefetchInProgress = true;
			prefetchAddress = pc + 2;
			val = read16(pc, AccessType::Prefetch);
		}
	}
	else
	{
		val = read16(pc, AccessType::Nonsequential);
		invalidatePrefetchBuffer();
		prefetchInProgress = true;
		prefetchAddress = pc + 2;
	}
	return val;
}

//Probably handle reading a single IO byte in 'readIO8'
//And then sort out 16/32 bit r/w using the above
uint8_t Bus::readIO8(uint32_t address)
{
	if (address <= 0x04000056)
		return m_ppu->readIO(address);
	if (address >= 0x04000130 && address <= 0x04000133)
		return m_input->readIORegister(address);
	if (address >= 0x040000B0 && address <= 0x040000DF)
		return DMARegRead(address);
	if (address >= 0x04000100 && address <= 0x0400010F)
		return m_timer->readIO(address);
	if (address >= 0x04000060 && address <= 0x040000A8)
		return m_apu->readIO(address);
	switch (address)
	{
	case 0x04000200:case 0x04000201: case 0x04000202:  case 0x04000203: case 0x04000208: case 0x04000209: case 0x0400020A: case 0x0400020B:
		return m_interruptManager->readIO(address);
	case 0x04000120: case 0x04000121: case 0x04000122: case 0x04000123: case 0x0400012A: case 0x04000128: case 0x04000129:
		return m_serial->readIO(address);
	case 0x04000204:
		return WAITCNT & 0xFF;
	case 0x04000205:
		return ((WAITCNT >> 8) & 0xFF);
	case 0x04000135:	//hack (tie top byte of rcnt to 0x80)
		return 0x80;
	}
	return m_openBusVals.mem >> ((address & 0b1)*8);
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
	if (address >= 0x040000B0 && address <= 0x040000DF)
	{
		DMARegWrite(address,value);
		return;
	}
	if (address >= 0x04000100 && address <= 0x0400010F)
	{
		m_timer->writeIO(address, value);
		return;
	}
	if (address >= 0x04000060 && address <= 0x040000A8)
	{
		m_apu->writeIO(address, value);
		return;
	}
	switch (address)
	{
	case 0x04000200:case 0x04000201: case 0x04000202:  case 0x04000203: case 0x04000208: case 0x04000209: case 0x0400020A: case 0x0400020B:
		m_interruptManager->writeIO(address,value);
		return;
	case 0x04000120: case 0x04000121: case 0x04000122: case 0x04000123: case 0x0400012A: case 0x04000128: case 0x04000129:
		m_serial->writeIO(address, value);
		break;
	case 0x04000204:
		WAITCNT &= 0xFF00; WAITCNT |= value;
		return;
	case 0x04000205:
		WAITCNT &= 0xFF; WAITCNT |= (value << 8);

		waitstateNonsequentialTable[0] = nonseqLUT[((WAITCNT >> 2) & 0b11)];
		waitstateNonsequentialTable[1] = nonseqLUT[((WAITCNT >> 5) & 0b11)];
		waitstateNonsequentialTable[2] = nonseqLUT[((WAITCNT >> 8) & 0b11)];
		waitstateSequentialTable[0] = ((WAITCNT >> 4) & 0b1) ? 1 : 2;
		waitstateSequentialTable[1] = ((WAITCNT >> 7) & 0b1) ? 1 : 4;
		waitstateSequentialTable[2] = ((WAITCNT >> 10) & 0b1) ? 1 : 8;
		SRAMCycles = nonseqLUT[(WAITCNT & 0b11)];
		prefetchEnabled = ((WAITCNT >> 14) & 0b1);

		return;
	case 0x04000301:
		while (!m_interruptManager->getInterrupt(true))
			m_scheduler->jumpToNextEvent();			//teleport to next event(s) until interrupt fires
		break;
	}
	//Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unimplemented IO write addr={:#x}", address));
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

bool Bus::getHalted()
{
	bool isHalted = shouldHalt;
	shouldHalt = false;
	return isHalted;
}


//Handles reading/writing larger than byte sized values (the addresses should already be aligned so no issues there)
//This is SOLELY for memory - IO is handled differently bc it's not treated as a flat mem space
uint16_t Bus::getValue16(uint8_t* arr, int base, int mask)
{
	return (uint16_t)arr[base] | ((arr[(base + 1)&mask]) << 8);
}

void Bus::setValue16(uint8_t* arr, int base, int mask, uint16_t val)
{
	arr[base] = val & 0xFF;
	arr[(base + 1)&mask] = ((val >> 8) & 0xFF);
}

uint32_t Bus::getValue32(uint8_t* arr, int base, int mask)
{
	return (uint32_t)arr[base] | ((arr[(base + 1)&mask]) << 8) | ((arr[(base + 2)&mask]) << 16) | ((arr[(base + 3)&mask]) << 24);
}

void Bus::setValue32(uint8_t* arr, int base, int mask, uint32_t val)
{
	arr[base] = val & 0xFF;
	arr[(base + 1)&mask] = ((val >> 8) & 0xFF);
	arr[(base + 2)&mask] = ((val >> 16) & 0xFF);
	arr[(base + 3)&mask] = ((val >> 24) & 0xFF);
}

void Bus::setByteInWord(uint32_t* word, uint8_t byte, int pos)
{
	uint32_t tmp = *word;
	uint32_t mask = 0xFF;
	mask = ~(mask << (pos * 8));
	tmp &= mask;
	tmp |= (byte << (pos * 8));
	*word = tmp;
}

void Bus::setByteInHalfword(uint16_t* halfword, uint8_t byte, int pos)
{
	uint16_t tmp = *halfword;
	uint16_t mask = 0xFF;
	mask = ~(mask << (pos * 8));
	tmp &= mask;
	tmp |= (byte << (pos * 8));
	*halfword = tmp;
}

void Bus::tickPrefetcher(uint64_t cycles)
{
	uint8_t page = (prefetchAddress >> 24) & 0xFF;
	uint64_t waitstates = waitstateSequentialTable[((page - 8) >> 1)];
	prefetchTargetCycles = waitstates;
	if (prefetchInProgress && prefetchEnabled && !prefetcherHalted)
	{
		prefetchInternalCycles += cycles;
		while (prefetchInternalCycles > waitstates && prefetchSize < 8)
		{
			prefetchInternalCycles -= waitstates;
			uint16_t val = read16(prefetchAddress, AccessType::Prefetch);
			m_prefetchBuffer[prefetchEnd] = { prefetchAddress,val };
			prefetchEnd = (prefetchEnd + 1) & 7;
			prefetchSize++;
			prefetchAddress += 2;
		}
		prefetchShouldDelay = ((prefetchTargetCycles - prefetchInternalCycles) == 0);
	}
}

void Bus::invalidatePrefetchBuffer()
{
	if (dmaInProgress)
	{
		prefetcherHalted = true;
		return;
	}
	prefetchInProgress = false;
	prefetchStart = 0;
	prefetchEnd = 0;
	prefetchSize = 0;
	prefetchInternalCycles = 0;
	prefetchShouldDelay = false;
}

void* Bus::busCtx = nullptr;