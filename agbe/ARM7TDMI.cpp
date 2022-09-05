#include"ARM7TDMI.h"

ARM7TDMI::ARM7TDMI(std::shared_ptr<Bus> bus, std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<Scheduler> scheduler)
{
	m_bus = bus;
	m_interruptManager = interruptManager;
	m_scheduler = scheduler;
	CPSR = 0x13;				//starts in svc mode upon boot?
	m_lastCheckModeBits = 0x13;
	for (int i = 0; i < 16; i++)
		R[i] = 0;

	R[15] = 0x00000000;
	flushPipeline();
	refillPipeline();
	m_pipelineFlushed = false;
	nextFetchNonsequential = true;

}

ARM7TDMI::~ARM7TDMI()
{

}

consteval std::array<ARM7TDMI::instructionFn, 4096> ARM7TDMI::genARMTable()
{
	std::array<instructionFn, 4096> armTable;
	armTable.fill((instructionFn)&ARM7TDMI::ARM_Undefined);
	//bypass compiler recursion limit by splitting up into 256 long chunks of filling the table
	setARMTableEntries<0, 256>(armTable);
	setARMTableEntries<256, 512>(armTable);
	setARMTableEntries<512, 768>(armTable);
	setARMTableEntries<768, 1024>(armTable);
	setARMTableEntries<1024, 1280>(armTable);
	setARMTableEntries<1280, 1536>(armTable);
	setARMTableEntries<1536, 1792>(armTable);
	setARMTableEntries<1792, 2048>(armTable);
	setARMTableEntries<2048, 2304>(armTable);
	setARMTableEntries<2304, 2560>(armTable);
	setARMTableEntries<2560, 2816>(armTable);
	setARMTableEntries<2816, 3072>(armTable);
	setARMTableEntries<3072, 3328>(armTable);
	setARMTableEntries<3328, 3584>(armTable);
	setARMTableEntries<3584, 3840>(armTable);
	setARMTableEntries<3840, 4096>(armTable);

	return armTable;
}

consteval std::array<ARM7TDMI::instructionFn, 1024> ARM7TDMI::genThumbTable()
{
	std::array<instructionFn, 1024> thumbTable;
	thumbTable.fill((instructionFn)&ARM7TDMI::ARM_Undefined);
	setThumbTableEntries<0, 256>(thumbTable);
	setThumbTableEntries<256, 512>(thumbTable);
	setThumbTableEntries<512, 768>(thumbTable);
	setThumbTableEntries<768, 1024>(thumbTable);
	return thumbTable;
}

void ARM7TDMI::step()
{
	fetch();
	if (dispatchInterrupt())	//if interrupt was dispatched then fetch new opcode (dispatchInterrupt already flushes pipeline !)
		return;
	execute();	//no decode stage because it's inherent to 'execute' - we accommodate for the decode stage's effect anyway
	if (m_pipelineFlushed)
	{
		refillPipeline();
		return;
	}

	R[15] += incrAmountLUT[m_inThumbMode];
	m_pipelinePtr = ((m_pipelinePtr + 1) % 3);
	m_pipelineFlushed = false;
	m_scheduler->tick();
}

void ARM7TDMI::fetch()
{
	int curPipelinePtr = m_pipelinePtr;
	m_pipeline[curPipelinePtr].state = PipelineState::FILLED;
	if (m_inThumbMode)
		m_pipeline[curPipelinePtr].opcode = m_bus->fetch16(R[15],(AccessType)!nextFetchNonsequential);
	else
		m_pipeline[curPipelinePtr].opcode = m_bus->fetch32(R[15],(AccessType)!nextFetchNonsequential);

	nextFetchNonsequential = false;
}

void ARM7TDMI::execute()
{
	int curPipelinePtr = (m_pipelinePtr + 1) % 3;	//+1 so when it wraps it's actually 2 behind the current fetch. e.g. cur fetch = 2, then cur execute = 0 (2 behind)
	pipelineFull = true;
	//NOTE: PC is 8 bytes ahead of opcode being executed

	m_currentOpcode = m_pipeline[curPipelinePtr].opcode;
	if (m_inThumbMode)	//thumb mode? pass over to different function to decode
	{
		executeThumb();
		return;
	}

	//check conditions before executing
	uint8_t conditionCode = ((m_currentOpcode >> 28) & 0xF);
	static constexpr auto conditionLUT = genConditionCodeTable();
	if (!conditionLUT[(CPSR>>28)&0xF][conditionCode]) [[unlikely]]
	{
		m_scheduler->addCycles(1);
		return;
	}

	static constexpr auto armTable = genARMTable();
	uint32_t lookup = ((m_currentOpcode & 0x0FF00000) >> 16) | ((m_currentOpcode & 0xF0) >> 4);	//bits 20-27 shifted down to bits 4-11. bits 4-7 shifted down to bits 0-4
	instructionFn instr = armTable[lookup];
	(this->*instr)();
}

void ARM7TDMI::executeThumb()
{
	static constexpr auto thumbTable = genThumbTable();
	uint16_t lookup = m_currentOpcode >> 6;
	instructionFn instr = thumbTable[lookup];
	(this->*instr)();
}

bool ARM7TDMI::dispatchInterrupt()
{
	if (((CPSR>>7)&0b1) || !m_interruptManager->getInterrupt() || !m_interruptManager->getInterruptsEnabled())
		return false;	//only dispatch if pipeline full (or not about to flush)
	//irq bits: 10010
	uint32_t oldCPSR = CPSR;
	CPSR &= ~0x3F;
	CPSR |= 0x92;
	m_inThumbMode = false;
	swapBankedRegisters();

	bool wasThumb = ((oldCPSR >> 5) & 0b1);
	constexpr int pcOffsetAmount[2] = { 4,0 };
	setSPSR(oldCPSR);
	setReg(14, getReg(15) - pcOffsetAmount[wasThumb]);
	setReg(15, 0x00000018);
	flushPipeline();
	refillPipeline();
	return true;
}

void ARM7TDMI::flushPipeline()
{
	m_pipelineFlushed = true;
	m_bus->invalidatePrefetchBuffer();
	pipelineFull = false;
}

void ARM7TDMI::refillPipeline()
{
	m_pipelineFlushed = false;

	switch (m_inThumbMode)
	{
	case 0:		//refill ARM
		m_pipeline[0].opcode = m_bus->fetch32(R[15], AccessType::Nonsequential);
		m_pipeline[1].opcode = m_bus->fetch32(R[15] + 4, AccessType::Sequential);
		R[15] += 8;
		break;
	case 1:		//refill thumb
		m_pipeline[0].opcode = m_bus->fetch16(R[15], AccessType::Nonsequential);
		m_pipeline[1].opcode = m_bus->fetch16(R[15] + 2, AccessType::Sequential);
		R[15] += 4;
		break;
	}

	nextFetchNonsequential = false;					//just so timing doesn't go wonky.
	m_pipeline[0].state = PipelineState::FILLED;
	m_pipeline[1].state = PipelineState::FILLED;
	m_pipeline[2].state = PipelineState::UNFILLED;	//this will be filled upon next FDE cycle
	m_pipelinePtr = 2;

	m_scheduler->tick();
}

//misc flag stuff
bool ARM7TDMI::m_getNegativeFlag()
{
	return (CPSR >> 31) & 0b1;
}

bool ARM7TDMI::m_getZeroFlag()
{
	return (CPSR >> 30) & 0b1;
}

bool ARM7TDMI::m_getCarryFlag()
{
	return (CPSR >> 29) & 0b1;
}

bool ARM7TDMI::m_getOverflowFlag()
{
	return (CPSR >> 28) & 0b1;
}

void ARM7TDMI::m_setNegativeFlag(bool value)
{
	uint32_t mask = (1 << 31);
	if (value)
		CPSR |= mask;
	else
		CPSR &= (~mask);
}

void ARM7TDMI::m_setZeroFlag(bool value)
{
	uint32_t mask = (1 << 30);
	if (value)
		CPSR |= mask;
	else
		CPSR &= (~mask);
}

void ARM7TDMI::m_setCarryFlag(bool value)
{
	uint32_t mask = (1 << 29);
	if (value)
		CPSR |= mask;
	else
		CPSR &= (~mask);
}

void ARM7TDMI::m_setOverflowFlag(bool value)
{
	uint32_t mask = (1 << 28);
	if (value)
		CPSR |= mask;
	else
		CPSR &= (~mask);
}

uint32_t ARM7TDMI::getReg(uint8_t reg)
{
	return R[reg];
}

void ARM7TDMI::setReg(uint8_t reg, uint32_t value)
{
	R[reg] = value;
	if (reg == 15)
		flushPipeline();
}

void ARM7TDMI::swapBankedRegisters()
{
	uint8_t oldMode = m_lastCheckModeBits;
	uint8_t newMode = CPSR & 0x1F;
	m_lastCheckModeBits = newMode;

	if ((oldMode == newMode) || (oldMode == 0b10000 && newMode == 0b11111) || (oldMode == 0b11111 && newMode == 0b10000))
		return;

	//first, save registers back to correct bank
	uint32_t* srcPtr = nullptr;
	switch (oldMode)
	{
	case 0b10000:
		srcPtr = usrBankedRegisters; break;
	case 0b10001:
		srcPtr = fiqBankedRegisters;
		memcpy(fiqExtraBankedRegisters, &R[8], 5 * sizeof(uint32_t));	//save FIQ banked R8-R12
		R[8] = usrExtraBankedRegisters[0];          //then load original R8-R12 before banking
		R[9] = usrExtraBankedRegisters[1];
		R[10] = usrExtraBankedRegisters[2];
		R[11] = usrExtraBankedRegisters[3];
		R[12] = usrExtraBankedRegisters[4];
		break;
	case 0b10010:
		srcPtr = irqBankedRegisters; break;
	case 0b10011:
		srcPtr = svcBankedRegisters; break;
	case 0b10111:
		srcPtr = abtBankedRegisters; break;
	case 0b11011:
		srcPtr = undBankedRegisters; break;
	case 0b11111:
		srcPtr = usrBankedRegisters; break;
	default:
		srcPtr = usrBankedRegisters; break;
	}

	memcpy(srcPtr,&R[13], 2 * sizeof(uint32_t));

	uint32_t* destPtr = nullptr;
	switch (newMode)
	{
	case 0b10000:
		destPtr = usrBankedRegisters; break;
	case 0b10001:
		destPtr = fiqBankedRegisters;
		//save R8-R12
		usrExtraBankedRegisters[0] = R[8];
		usrExtraBankedRegisters[1] = R[9];
		usrExtraBankedRegisters[2] = R[10];
		usrExtraBankedRegisters[3] = R[11];
		usrExtraBankedRegisters[4] = R[12];
		//then load in banked R8-R12
		memcpy(&R[8], fiqExtraBankedRegisters, 5 * sizeof(uint32_t));
		break;
	case 0b10010:
		destPtr = irqBankedRegisters; break;
	case 0b10011:
		destPtr = svcBankedRegisters; break;
	case 0b10111:
		destPtr = abtBankedRegisters; break;
	case 0b11011:
		destPtr = undBankedRegisters; break;
	case 0b11111:
		destPtr = usrBankedRegisters; break;
	default:
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Invalid destination mode: {:#x}", (int)newMode));
		destPtr = usrBankedRegisters;
		break;
	}

	memcpy(&R[13], destPtr, 2 * sizeof(uint32_t));
}

uint32_t ARM7TDMI::getSPSR()
{
	uint8_t mode = CPSR & 0x1F;
	switch (mode)
	{
	case 0b10001: return SPSR_fiq;
	case 0b10011: return SPSR_svc;
	case 0b10111: return SPSR_abt;
	case 0b10010: return SPSR_irq;
	case 0b11011: return SPSR_und;
	};
	return CPSR;	//afaik if you try this in the wrong mode it gives you the CPSR
}

void ARM7TDMI::setSPSR(uint32_t value)
{
	uint8_t mode = CPSR & 0x1F;
	switch (mode)
	{
	case 0b10001: SPSR_fiq = value; break;
	case 0b10011: SPSR_svc = value; break;
	case 0b10111: SPSR_abt = value; break;
	case 0b10010: SPSR_irq = value; break;
	case 0b11011: SPSR_und = value; break;
	default:break;
	}
}

int ARM7TDMI::calculateMultiplyCycles(uint32_t operand, bool isSigned)
{
	int totalCycles = 4;
	if (isSigned)
	{
		if ((operand >> 8) == 0xFFFFFF)
			totalCycles = 1;
		else if ((operand >> 16) == 0xFFFF)
			totalCycles = 2;
		else if ((operand >> 24) == 0xFF)
			totalCycles = 3;
	}

	if ((operand >> 8) == 0)
		totalCycles = 1;
	else if ((operand >> 16) == 0)
		totalCycles = 2;
	else if ((operand >> 24) == 0)
		totalCycles = 3;
	return totalCycles;
}