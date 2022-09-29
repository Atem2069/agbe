#include"Bus.h"

Bus::Bus(std::vector<uint8_t> BIOS, std::vector<uint8_t> cartData, std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<PPU> ppu, std::shared_ptr<Input> input, std::shared_ptr<Scheduler> scheduler)
{
	m_scheduler = scheduler;
	m_interruptManager = interruptManager;
	m_ppu = ppu;
	m_input = input;

	m_mem = std::make_shared<GBAMem>();
	m_timer = std::make_shared<Timer>(m_interruptManager,m_scheduler);
	m_apu = std::make_shared<APU>(m_scheduler);
	m_timer->registerAPUCallbacks((callbackFn)&APU::timer0Callback, (callbackFn)&APU::timer1Callback, (void*)m_apu.get());
	m_apu->registerDMACallback((FIFOcallbackFn)&Bus::DMA_AudioFIFOCallback, (void*)this);
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

	if (romSize == 1048576)
		romAddressMask = 1048575;	//classic nes games have mirrored rom, instead of 'normal' OOB ROM access behaviour
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
				return std::rotr(m_openBusVals.bios, 8*(address&0b11));	//todo: account for the value being rotated properly
			return std::rotr(m_openBusVals.mem, 8*(address&0b11));
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
		if ((address & romAddressMask) >= romSize)
			return std::rotr((address / 2) & 0xFFFF, 8 * (address & 0b11));
		return m_mem->ROM[address & romAddressMask];
	case 0xE: case 0xF:
		m_scheduler->addCycles(SRAMCycles);	//hm.
		if (prefetchInProgress && prefetchShouldDelay)
			m_scheduler->addCycles(1);
		prefetchShouldDelay = false;
		invalidatePrefetchBuffer();
		if (m_backupType == BackupType::FLASH1M || m_backupType == BackupType::FLASH512K || m_backupType == BackupType::SRAM)
			return m_backupMemory->read(address);
	}

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

		//8/16 bit writes to audio fifos will cause an entire word to be pushed, weird.
		if (address>=0x040000A0 && address<=0x040000A7)
			m_apu->advanceSamplePtr((address&7) >>2);

		break;
	case 5:
		tickPrefetcher(1);
		m_mem->paletteRAM[address & 0x3FF] = value;
		m_mem->paletteRAM[(address + 1) & 0x3FF] = value;
		break;
	case 6:
		tickPrefetcher(1);
		address = address & 0x1FFFF;
		if (address >= 0x10000)
			break;
		m_mem->VRAM[address]=value;
		m_mem->VRAM[address + 1] = value;
		break;
	case 7:
		tickPrefetcher(1);
		break;
	case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD:
		cartCycles = ((accessType==AccessType::Sequential) && ((address & 0x1FF) != 0)) ? waitstateSequentialTable[((page - 8) >> 1)] : waitstateNonsequentialTable[((page - 8) >> 1)];
		m_scheduler->addCycles(cartCycles);
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
		if ((address & romAddressMask) >= romSize)
			return (address / 2) & 0xFFFF;
		return getValue16(m_mem->ROM, address & romAddressMask,0xFFFFFFFF);
	case 0xE: case 0xF:
		m_scheduler->addCycles(SRAMCycles);
		if (m_backupType == BackupType::SRAM)
			return ((uint16_t)m_backupMemory->read(originalAddress)) * 0x0101;
	}

	tickPrefetcher(1);

	if (m_openBusVals.dmaJustFinished)
	{
		m_openBusVals.dmaJustFinished = false;
		return m_openBusVals.lastDmaVal;
	}

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

		//similar for 8 bit writes, but 16 bit writes will modify part of the current word in the audio fifo, then advance the write ptr to the next word..
		if ((address == 0x040000A0 || address == 0x040000A2 || address == 0x040000A4 || address == 0x040000A6))
			m_apu->advanceSamplePtr((address & 7) >> 2);

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
		break;
	case 0xE: case 0xF:
		m_scheduler->addCycles(SRAMCycles);
		if (m_backupType == BackupType::SRAM)
		{
			value = (std::rotr(value, (originalAddress * 8))) & 0xFF;
			m_backupMemory->write(originalAddress, value);
		}
		break;
	default:
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
				m_scheduler->addCycles(1);
			prefetchShouldDelay = false;
			invalidatePrefetchBuffer();
		}
		if ((address & romAddressMask) >= romSize)
			return ((address / 2) & 0xFFFF) | (((address + 2) / 2) & 0xFFFF) << 16;
		return getValue32(m_mem->ROM, address & romAddressMask, 0xFFFFFFFF);
	case 0xE: case 0xF:
		m_scheduler->addCycles(SRAMCycles);
		if (m_backupType == BackupType::SRAM)
			return ((uint32_t)m_backupMemory->read(originalAddress)) * 0x01010101;
	}

	tickPrefetcher(1);
	if (m_openBusVals.dmaJustFinished)
	{
		m_openBusVals.dmaJustFinished = false;
		return m_openBusVals.lastDmaVal;
	}
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

		//standard behaviour for audio fifos - i.e. write and push an entire word at once
		if ((address == 0x040000A0 || address == 0x040000A4))
			m_apu->advanceSamplePtr((address & 7) >> 2);

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
			m_scheduler->addCycles(1);
		prefetchShouldDelay = false;
		invalidatePrefetchBuffer();
		break;
	case 0xE: case 0xF:
		m_scheduler->addCycles(SRAMCycles);
		if (m_backupType == BackupType::SRAM)
			m_backupMemory->write(originalAddress, (std::rotr(value, originalAddress * 8) & 0xFF));
		break;
	default:
		tickPrefetcher(1);
		break;
	}
}

uint32_t Bus::fetch32(uint32_t address, AccessType accessType)
{
	if (hack_forceNonseq && !prefetchEnabled)
	{
		accessType = AccessType::Nonsequential;
		hack_forceNonseq = false;
	}
	biosLockout = (address > 0x3FFF);
	if (address < 0x08000000 || address > 0x0DFFFFFF)
		invalidatePrefetchBuffer();
	uint32_t val = 0;
	if ((prefetchEnabled || prefetchSize>0) && address >= 0x08000000 && address <= 0x0DFFFFFF)
	{
		if (prefetchInProgress)
		{
			if (prefetchSize > 0)	//hmm.. seems like prefetcher always ticked even if only one halfword is loaded?
				tickPrefetcher(1);
			uint16_t valLow = getPrefetchedValue(address);
			uint16_t valHigh = getPrefetchedValue(address + 2);
			val = ((valHigh << 16) | valLow);
			if (!hack_lastPrefetchGood)
				m_scheduler->addCycles(1);		//not sure: seems like a cycle added if we end up having to do a halfword fetch.. :(
		}
		else
		{
			prefetchShouldDelay = false;
			val = read32(address, AccessType::Nonsequential);
			invalidatePrefetchBuffer();
			prefetchInProgress = true;
			prefetchAddress = address + 4;
			m_prefetchHead = prefetchAddress;
		}
		m_openBusVals.mem = val;
	}
	else
		val = read32(address,accessType);
	m_openBusVals.mem = val;
	return val;
}

uint16_t Bus::fetch16(uint32_t address, AccessType accessType)
{
	if (hack_forceNonseq && !prefetchEnabled)
	{
		accessType = AccessType::Nonsequential;
		hack_forceNonseq = false;
	}
	biosLockout = (address > 0x3FFF);
	if (address < 0x08000000 || address > 0x0DFFFFFF)
		invalidatePrefetchBuffer();
	uint16_t val = 0;
	if ((prefetchEnabled || prefetchSize>0) && address >= 0x08000000 && address <= 0x0DFFFFFF)	//nice, we can just read the prefetch buffer
	{
		if (prefetchSize > 0)
			tickPrefetcher(1);
		val = getPrefetchedValue(address);
	}
	else
		val = read16(address, accessType);

	//ugh.... there has to be a nicer way to do this, but..
	//this boils down to there being different rules for thumb open bus alignment
	switch (((address >> 24) & 0xFF))
	{
		//main ram, palette ram, vram, cart (maybe sram too? doublecheck !!)
	case 2: case 5: case 6: case 8: case 9: case 0xA: case 0xB: case 0xC: case 0xD: case 0xE: case 0xF:
		m_openBusVals.mem = (val << 16) | val;
		break;
		//bios,oam (wtf)
	case 0: case 7:
		if ((address >> 1) & 0b1)
			m_openBusVals.mem = (val << 16) | (read16(address - 2, AccessType::Prefetch));
		else
			m_openBusVals.mem = (read16(address + 2, AccessType::Prefetch) << 16) | val;
		break;
	case 3:
		if ((address >> 1) & 0b1)
		{
			m_openBusVals.mem &= 0xFF;
			m_openBusVals.mem |= (val << 16);
		}
		else
		{
			m_openBusVals.mem &= 0xFF00;
			m_openBusVals.mem |= val;
		}
		break;
	}
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
			if (m_prefetchHead != pc)
				Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Prefetcher/CPU misalign: expected addr={:#x}, prefetcher addr = {:#x}", pc, m_prefetchHead));
			val = read16(pc, AccessType::Prefetch);
			prefetchStart = (prefetchStart + 1) & 7;
			prefetchSize--;
			m_prefetchHead += 2;
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
			m_prefetchHead = prefetchAddress;
			val = read16(pc, AccessType::Prefetch);
		}

		if (prefetchSize == 0)
			hack_forceNonseq = true;
	}
	else
	{
		val = read16(pc, AccessType::Nonsequential);
		invalidatePrefetchBuffer();
		prefetchInProgress = true;
		prefetchAddress = pc + 2;
		m_prefetchHead = prefetchAddress;
	}
	return val;
}

//Probably handle reading a single IO byte in 'readIO8'
//And then sort out 16/32 bit r/w using the above
uint8_t Bus::readIO8(uint32_t address)
{
	switch (address)
	{
	case 0x04000000: case 0x04000001: case 0x04000002: case 0x04000003: case 0x04000004: case 0x04000005: case 0x04000006: case 0x04000007:
	case 0x04000008: case 0x04000009: case 0x0400000A: case 0x0400000B: case 0x0400000C: case 0x0400000D: case 0x0400000E: case 0x0400000F:
	case 0x04000048: case 0x04000049: case 0x0400004A: case 0x0400004B: case 0x04000050: case 0x04000051: case 0x04000052: case 0x04000053:
		return m_ppu->readIO(address);
	case 0x04000060: case 0x04000061: case 0x04000062: case 0x04000063: case 0x04000064: case 0x04000065: case 0x04000066: case 0x04000067:
	case 0x04000068: case 0x04000069: case 0x0400006a: case 0x0400006b: case 0x0400006c: case 0x0400006d: case 0x0400006e: case 0x0400006f:
	case 0x04000070: case 0x04000071: case 0x04000072: case 0x04000073: case 0x04000074: case 0x04000075: case 0x04000076: case 0x04000077:
	case 0x04000078: case 0x04000079: case 0x0400007a: case 0x0400007b: case 0x0400007c: case 0x0400007d: case 0x0400007e: case 0x0400007f:
	case 0x04000080: case 0x04000081: case 0x04000082: case 0x04000083: case 0x04000084: case 0x04000085: case 0x04000086: case 0x04000087:
	case 0x04000088: case 0x04000089: case 0x0400008a: case 0x0400008b:	//..8c,..8d,..8e,..8f aren't readable :(
	case 0x04000090: case 0x04000091: case 0x04000092: case 0x04000093: case 0x04000094: case 0x04000095: case 0x04000096: case 0x04000097:
	case 0x04000098: case 0x04000099: case 0x0400009a: case 0x0400009b: case 0x0400009c: case 0x0400009d: case 0x0400009e: case 0x0400009f:
		return m_apu->readIO(address);
	case 0x040000B8: case 0x040000B9: case 0x040000BA: case 0x040000BB: case 0x040000C4: case 0x040000C5: case 0x040000C6: case 0x040000C7:
	case 0x040000D0: case 0x040000D1: case 0x040000D2: case 0x040000D3: case 0x040000DC: case 0x040000DD: case 0x040000DE: case 0x040000DF:
		return DMARegRead(address);
	case 0x04000100: case 0x04000101: case 0x04000102: case 0x04000103: case 0x04000104: case 0x04000105: case 0x04000106: case 0x04000107:
	case 0x04000108: case 0x04000109: case 0x0400010a: case 0x0400010b: case 0x0400010c: case 0x0400010d: case 0x0400010e: case 0x0400010f:
		return m_timer->readIO(address);
	case 0x04000130: case 0x04000131: case 0x04000132: case 0x04000133:
		return m_input->readIORegister(address);
	case 0x04000200:case 0x04000201: case 0x04000202:  case 0x04000203: case 0x04000208: case 0x04000209: case 0x0400020A: case 0x0400020B:
		return m_interruptManager->readIO(address);
	case 0x04000120: case 0x04000121: case 0x04000122: case 0x04000123: case 0x0400012A: case 0x04000128: case 0x04000129:
		return m_serial->readIO(address);
	case 0x04000204:
		return WAITCNT & 0xFF;
	case 0x04000205:
		return ((WAITCNT >> 8) & 0x7F);		//assume bit 15 basically always 0 (would only get set if cgb/dmg game inserted into console)
	case 0x04000135:	//hack (tie top byte of rcnt to 0x80)
		return 0x80;
	case 0x04000300:
		return POSTFLG & 0b1;
	}
	return std::rotr(m_openBusVals.mem, (address & 0b11)*8);
}

void Bus::writeIO8(uint32_t address, uint8_t value)
{
	switch (address)
	{
	case 0x04000000: case 0x04000001: case 0x04000002: case 0x04000003: case 0x04000004: case 0x04000005: case 0x04000006: case 0x04000007: 
	case 0x04000008: case 0x04000009: case 0x0400000a: case 0x0400000b: case 0x0400000c: case 0x0400000d: case 0x0400000e: case 0x0400000f: 
	case 0x04000010: case 0x04000011: case 0x04000012: case 0x04000013: case 0x04000014: case 0x04000015: case 0x04000016: case 0x04000017: 
	case 0x04000018: case 0x04000019: case 0x0400001a: case 0x0400001b: case 0x0400001c: case 0x0400001d: case 0x0400001e: case 0x0400001f: 
	case 0x04000020: case 0x04000021: case 0x04000022: case 0x04000023: case 0x04000024: case 0x04000025: case 0x04000026: case 0x04000027: 
	case 0x04000028: case 0x04000029: case 0x0400002a: case 0x0400002b: case 0x0400002c: case 0x0400002d: case 0x0400002e: case 0x0400002f: 
	case 0x04000030: case 0x04000031: case 0x04000032: case 0x04000033: case 0x04000034: case 0x04000035: case 0x04000036: case 0x04000037: 
	case 0x04000038: case 0x04000039: case 0x0400003a: case 0x0400003b: case 0x0400003c: case 0x0400003d: case 0x0400003e: case 0x0400003f: 
	case 0x04000040: case 0x04000041: case 0x04000042: case 0x04000043: case 0x04000044: case 0x04000045: case 0x04000046: case 0x04000047: 
	case 0x04000048: case 0x04000049: case 0x0400004a: case 0x0400004b: case 0x0400004c: case 0x0400004d: case 0x0400004e: case 0x0400004f: 
	case 0x04000050: case 0x04000051: case 0x04000052: case 0x04000053: case 0x04000054: case 0x04000055: case 0x04000056:
		m_ppu->writeIO(address, value);
		return;
	case 0x04000060: case 0x04000061: case 0x04000062: case 0x04000063: case 0x04000064: case 0x04000065: case 0x04000066: case 0x04000067:
	case 0x04000068: case 0x04000069: case 0x0400006a: case 0x0400006b: case 0x0400006c: case 0x0400006d: case 0x0400006e: case 0x0400006f:
	case 0x04000070: case 0x04000071: case 0x04000072: case 0x04000073: case 0x04000074: case 0x04000075: case 0x04000076: case 0x04000077:
	case 0x04000078: case 0x04000079: case 0x0400007a: case 0x0400007b: case 0x0400007c: case 0x0400007d: case 0x0400007e: case 0x0400007f:
	case 0x04000080: case 0x04000081: case 0x04000082: case 0x04000083: case 0x04000084: case 0x04000085: case 0x04000086: case 0x04000087:
	case 0x04000088: case 0x04000089: case 0x0400008a: case 0x0400008b: case 0x0400008c: case 0x0400008d: case 0x0400008e: case 0x0400008f:
	case 0x04000090: case 0x04000091: case 0x04000092: case 0x04000093: case 0x04000094: case 0x04000095: case 0x04000096: case 0x04000097:
	case 0x04000098: case 0x04000099: case 0x0400009a: case 0x0400009b: case 0x0400009c: case 0x0400009d: case 0x0400009e: case 0x0400009f:
	case 0x040000a0: case 0x040000a1: case 0x040000a2: case 0x040000a3: case 0x040000a4: case 0x040000a5: case 0x040000a6: case 0x040000a7:
	case 0x040000a8:
		m_apu->writeIO(address, value);
		return;
	case 0x040000b0: case 0x040000b1: case 0x040000b2: case 0x040000b3: case 0x040000b4: case 0x040000b5: case 0x040000b6: case 0x040000b7:
	case 0x040000b8: case 0x040000b9: case 0x040000ba: case 0x040000bb: case 0x040000bc: case 0x040000bd: case 0x040000be: case 0x040000bf:
	case 0x040000c0: case 0x040000c1: case 0x040000c2: case 0x040000c3: case 0x040000c4: case 0x040000c5: case 0x040000c6: case 0x040000c7:
	case 0x040000c8: case 0x040000c9: case 0x040000ca: case 0x040000cb: case 0x040000cc: case 0x040000cd: case 0x040000ce: case 0x040000cf:
	case 0x040000d0: case 0x040000d1: case 0x040000d2: case 0x040000d3: case 0x040000d4: case 0x040000d5: case 0x040000d6: case 0x040000d7: 
	case 0x040000d8: case 0x040000d9: case 0x040000da: case 0x040000db: case 0x040000dc: case 0x040000dd: case 0x040000de: case 0x040000df:
		DMARegWrite(address, value);
		return;
	case 0x04000100: case 0x04000101: case 0x04000102: case 0x04000103: case 0x04000104: case 0x04000105: case 0x04000106: case 0x04000107:
	case 0x04000108: case 0x04000109: case 0x0400010a: case 0x0400010b: case 0x0400010c: case 0x0400010d: case 0x0400010e: case 0x0400010f:
		m_timer->writeIO(address, value);
		return;
	case 0x04000130: case 0x04000131: case 0x04000132: case 0x04000133:
		m_input->writeIORegister(address, value);
		return;
	case 0x04000200: case 0x04000201: case 0x04000202: case 0x04000203: case 0x04000208: case 0x04000209: case 0x0400020A: case 0x0400020B:
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
		if (prefetchEnabled && prefetchInProgress && !(((WAITCNT >> 14) & 0b1)))
		{
			m_scheduler->addCycles((prefetchTargetCycles - prefetchInternalCycles));
			prefetchSize++;
		}
		prefetchEnabled = ((WAITCNT >> 14) & 0b1);

		return;
	case 0x04000300:
		if(!POSTFLG)			//<--not sure, maybe POSTFLG can only be set once
		POSTFLG = value & 0b1;
		return;
	case 0x04000301:
		if (biosLockout)			//HALTCNT can't be written outside of BIOS
			return;
		haltSystem(((value>>7)&0b1));	//0=halt,1=stop 
		return;
	}
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
}

void Bus::writeIO32(uint32_t address, uint32_t value)
{
	writeIO16(address, value & 0xFFFF);
	writeIO16(address + 2, ((value >> 16) & 0xFFFF));
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

void Bus::haltSystem(bool stop)
{
	if (stop)	//TODO: research this on real hardware. (e.g. if IF gets set if KEYCNT.14 not enabled??)
	{
		m_ppu->reset();	//display disabled on real hardware, so set screen to all black
		Logger::getInstance()->msg(LoggerSeverity::Info, "STOP mode entered. ");
		while (!m_input->getIRQConditionsMet() && !Config::GBA.shouldReset)	//<-- potentially game pak or SIO irq could exit stop
			m_input->tick();												//but fwiw games only really use stop for 'sleep mode', exited thru the joypad
		return;
	}
	m_scheduler->addCycles(2);	//2 cycle penalty (one before, one after?) when haltcnt written
	while (!m_interruptManager->getInterrupt() && !Config::GBA.shouldReset)
		m_scheduler->jumpToNextEvent();			//teleport to next event(s) until interrupt fires
}

void Bus::tickPrefetcher(uint64_t cycles)
{
	uint8_t page = (prefetchAddress >> 24) & 0xFF;
	uint64_t waitstates = waitstateSequentialTable[((page - 8) >> 1)]+1;
	prefetchTargetCycles = waitstates;
	if (prefetchInProgress && prefetchEnabled && !prefetcherHalted)
	{
		prefetchInternalCycles += cycles;
		while (prefetchInternalCycles >= waitstates && prefetchSize < 8)
		{
			prefetchInternalCycles -= waitstates;
			prefetchEnd = (prefetchEnd + 1) & 7;
			prefetchSize++;
			prefetchAddress += 2;
		}
		prefetchShouldDelay = ((prefetchTargetCycles - prefetchInternalCycles) == 1);
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
