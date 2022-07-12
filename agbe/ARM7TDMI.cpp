#include"ARM7TDMI.h"

ARM7TDMI::ARM7TDMI(std::shared_ptr<Bus> bus, std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<Scheduler> scheduler)
{
	m_bus = bus;
	m_interruptManager = interruptManager;
	m_scheduler = scheduler;
	CPSR = 0x1F;	//system starts in system mode
	for (int i = 0; i < 16; i++)
		R[i] = 0;

	//R[13] = 0x03007F00;
	//R13_irq = 0x03007FA0;
	//R13_svc = 0x03007FE0;
	R[15] = 0x00000000;	//start of cartridge
	flushPipeline();
	m_pipelineFlushed = false;
	nextFetchNonsequential = true;
}

ARM7TDMI::~ARM7TDMI()
{

}

void ARM7TDMI::step()
{
	fetch();
	if (dispatchInterrupt())	//if interrupt was dispatched then fetch new opcode (dispatchInterrupt already flushes pipeline !)
		return;
	execute();	//no decode stage because it's inherent to 'execute' - we accommodate for the decode stage's effect anyway
	if(!m_pipelineFlushed)											//essentially only advance pipeline stage if the last operation didn't cause a pipeline flush
	{
		int incrAmountLUT[2] = { 4,2 };
		bool thumb = (CPSR >> 5) & 0b1;	//increment pc only if pipeline not flushed
		R[15] += incrAmountLUT[thumb];
		m_pipelinePtr = ((m_pipelinePtr + 1) % 3);
	}
	m_pipelineFlushed = false;
	m_scheduler->tick();
}

void ARM7TDMI::fetch()
{
	bool thumb = (CPSR >> 5) & 0b1;
	int curPipelinePtr = m_pipelinePtr;
	m_pipeline[curPipelinePtr].state = PipelineState::FILLED;
	if (thumb)
		m_pipeline[curPipelinePtr].opcode = m_bus->fetch16(R[15],(AccessType)!nextFetchNonsequential);
	else
		m_pipeline[curPipelinePtr].opcode = m_bus->fetch32(R[15],(AccessType)!nextFetchNonsequential);

	nextFetchNonsequential = false;
}

void ARM7TDMI::execute()
{
	int curPipelinePtr = (m_pipelinePtr + 1) % 3;	//+1 so when it wraps it's actually 2 behind the current fetch. e.g. cur fetch = 2, then cur execute = 0 (2 behind)
	if (m_pipeline[curPipelinePtr].state == PipelineState::UNFILLED)	//return if we haven't put an opcode up to this point in the pipeline
		return;
	pipelineFull = true;
	//NOTE: PC is 8 bytes ahead of opcode being executed

	m_currentOpcode = m_pipeline[curPipelinePtr].opcode;
	if ((CPSR >> 5) & 0b1)	//thumb mode? pass over to different function to decode
	{
		executeThumb();
		return;
	}

	//check conditions before executing
	uint8_t conditionCode = ((m_currentOpcode >> 28) & 0xF);
	if (!checkConditions(conditionCode))
	{
		m_scheduler->addCycles(1);
		return;
	}

	uint32_t lookup = ((m_currentOpcode & 0x0FF00000) >> 16) | ((m_currentOpcode & 0xF0) >> 4);	//bits 20-27 shifted down to bits 4-11. bits 4-7 shifted down to bits 0-4
	instructionFn instr = armTable[lookup];
	(this->*instr)();
}

void ARM7TDMI::executeThumb()
{
	uint16_t lookup = m_currentOpcode >> 6;
	instructionFn instr = thumbTable[lookup];
	(this->*instr)();
}

bool ARM7TDMI::dispatchInterrupt()
{
	if (!pipelineFull || ((CPSR>>7)&0b1) || !m_interruptManager->getInterrupt(false))
		return false;	//only dispatch if pipeline full (or not about to flush)
	//irq bits: 10010
	uint32_t oldCPSR = CPSR;
	CPSR &= ~0x3F;
	CPSR |= 0x92;

	bool wasThumb = ((oldCPSR >> 5) & 0b1);
	constexpr int pcOffsetAmount[2] = { 4,0 };
	setSPSR(oldCPSR);
	setReg(14, getReg(15) - pcOffsetAmount[wasThumb]);
	setReg(15, 0x00000018);
	flushPipeline();
	return true;
}

void ARM7TDMI::flushPipeline()
{
	for (int i = 0; i < 3; i++)
		m_pipeline[i].state = PipelineState::UNFILLED;
	m_pipelinePtr = 0;
	m_pipelineFlushed = true;
	nextFetchNonsequential = true;
	m_bus->invalidatePrefetchBuffer();
	pipelineFull = false;
}

bool ARM7TDMI::checkConditions(uint8_t code)
{
	uint8_t opFlags = code; 
	switch (opFlags)
	{
	case 0: return m_getZeroFlag();
	case 1: return !m_getZeroFlag();
	case 2: return m_getCarryFlag();
	case 3: return !m_getCarryFlag();
	case 4: return m_getNegativeFlag();
	case 5: return !m_getNegativeFlag();
	case 6: return m_getOverflowFlag();
	case 7: return !m_getOverflowFlag();
	case 8: return (m_getCarryFlag() && !m_getZeroFlag());
	case 9: return ((!m_getCarryFlag()) || m_getZeroFlag());
	case 10: return ((m_getNegativeFlag() && m_getOverflowFlag()) || (!m_getNegativeFlag() && !m_getOverflowFlag()));
	case 11: return (m_getNegativeFlag() != m_getOverflowFlag());
	case 12: return !m_getZeroFlag() && (m_getNegativeFlag() == m_getOverflowFlag());
	case 13: return m_getZeroFlag() || (m_getNegativeFlag() != m_getOverflowFlag());
	case 14: return true;
	case 15: Logger::getInstance()->msg(LoggerSeverity::Error, "Invalid condition code 1111 !!!!"); break;
	}
	return true;
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

uint32_t ARM7TDMI::getReg(uint8_t reg, bool forceUser)
{
	uint8_t mode = CPSR & 0x1F;
	if (reg < 8 || forceUser)		//R0-R7 not banked so this is gucci
		return R[reg];
	switch (reg)
	{
	case 8:
		if (mode == 0b10001)
			return R8_fiq;
		return R[8];
	case 9:
		if (mode == 0b10001)
			return R9_fiq;
		return R[9];
	case 10:
		if (mode == 0b10001)
			return R10_fiq;
		return R[10];
	case 11:
		if (mode == 0b10001)
			return R11_fiq;
		return R[11];
	case 12:
		if (mode == 0b10001)
			return R12_fiq;
		return R[12];
	case 13:
		if (mode == 0b10001)
			return R13_fiq;
		if (mode == 0b10011)
			return R13_svc;
		if (mode == 0b10111)
			return R13_abt;
		if (mode == 0b10010)
			return R13_irq;
		if (mode == 0b11011)
			return R13_und;
		return R[13];
	case 14:
		if (mode == 0b10001)
			return R14_fiq;
		if (mode == 0b10011)
			return R14_svc;
		if (mode == 0b10111)
			return R14_abt;
		if (mode == 0b10010)
			return R14_irq;
		if (mode == 0b11011)
			return R14_und;
		return R[14];
	case 15:
		return R[15];
	}
}

void ARM7TDMI::setReg(uint8_t reg, uint32_t value, bool forceUser)
{
	uint8_t mode = CPSR & 0x1F;
	if (reg < 8 || forceUser)		//R0-R7 not banked so this is gucci
		R[reg] = value;
	switch (reg)
	{
	case 8:
		if (mode == 0b10001)
			R8_fiq = value;
		else
			R[8] = value;
		break;
	case 9:
		if (mode == 0b10001)
			R9_fiq = value;
		else
			R[9] = value;
		break;
	case 10:
		if (mode == 0b10001)
			R10_fiq = value;
		else
			R[10] = value;
		break;
	case 11:
		if (mode == 0b10001)
			R11_fiq = value;
		else
			R[11] = value;
		break;
	case 12:
		if (mode == 0b10001)
			R12_fiq = value;
		else
			R[12] = value;
		break;
	case 13:
		if (mode == 0b10001)
			R13_fiq = value;
		else if (mode == 0b10011)
			R13_svc = value;
		else if (mode == 0b10111)
			R13_abt = value;
		else if (mode == 0b10010)
			R13_irq = value;
		else if (mode == 0b11011)
			R13_und = value;
		else
			R[13] = value;
		break;
	case 14:
		if (mode == 0b10001)
			R14_fiq = value;
		else if (mode == 0b10011)
			R14_svc = value;
		else if (mode == 0b10111)
			R14_abt = value;
		else if (mode == 0b10010)
			R14_irq = value;
		else if (mode == 0b11011)
			R14_und = value;
		else
			R[14] = value;
		break;
	case 15:
		flushPipeline();	//always flush when r15 modified
		R[15] = value;
		break;
	}
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