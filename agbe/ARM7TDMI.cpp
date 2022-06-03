#include"ARM7TDMI.h"

ARM7TDMI::ARM7TDMI(std::shared_ptr<Bus> bus)
{
	m_bus = bus;
	CPSR = 0x1F;	//system starts in system mode
	for (int i = 0; i < 16; i++)
		R[i] = 0;
	R[13] = 0x03007F00;
	R13_irq = 0x03007FA0;
	R13_svc = 0x03007FE0;
	R[15] = 0x08000000;	//start of cartridge
	flushPipeline();
	m_shouldFlush = 0;
}

ARM7TDMI::~ARM7TDMI()
{

}

void ARM7TDMI::step()
{
	
	fetch();
	execute();	//no decode stage because it's inherent to 'execute' - we accommodate for the decode stage's effect anyway

	if (m_shouldFlush)
		flushPipeline();
	else												//essentially only advance pipeline stage if the last operation didn't cause a pipeline flush
	{
		bool thumb = (CPSR >> 5) & 0b1;	//increment pc only if pipeline not flushed
		if (thumb)
			R[15] += 2;
		else
			R[15] += 4;
		m_pipelinePtr = ((m_pipelinePtr + 1) % 3);
	}
}

void ARM7TDMI::fetch()
{
	bool thumb = (CPSR >> 5) & 0b1;
	int curPipelinePtr = m_pipelinePtr;
	m_pipeline[curPipelinePtr].state = PipelineState::FILLED;
	if (thumb)
		m_pipeline[curPipelinePtr].opcode = m_bus->read16(R[15]);
	else
		m_pipeline[curPipelinePtr].opcode = m_bus->read32(R[15]);
}

void ARM7TDMI::execute()
{
	int curPipelinePtr = (m_pipelinePtr + 1) % 3;	//+1 so when it wraps it's actually 2 behind the current fetch. e.g. cur fetch = 2, then cur execute = 0 (2 behind)
	if (m_pipeline[curPipelinePtr].state == PipelineState::UNFILLED)	//return if we haven't put an opcode up to this point in the pipeline
		return;
	//NOTE: PC is 12 bytes ahead of opcode being executed

	m_currentOpcode = m_pipeline[curPipelinePtr].opcode;
	if ((CPSR >> 5) & 0b1)	//thumb mode? pass over to different function to decode
	{
		executeThumb();
		return;
	}

	//check conditions before executing
	if (!checkConditions())
		return;

	Logger::getInstance()->msg(LoggerSeverity::Info, std::format("Execute opcode (ARM) {:#x}. PC={:#x}", m_currentOpcode, R[15]-8));

	//decode instruction here	(we'll clean up binary masks when this works probs)
	if ((m_currentOpcode & 0b0000'1110'0000'0000'0000'0000'0000'0000) == 0b0000'1010'0000'0000'0000'0000'0000'0000)
		ARM_Branch();
	else if ((m_currentOpcode & 0b0000'1111'1100'0000'0000'0000'1111'0000) == 0b0000'0000'0000'0000'0000'0000'1001'0000)
		ARM_Multiply();
	else if ((m_currentOpcode & 0b0000'1111'1000'0000'0000'0000'1111'0000) == 0b0000'0000'1000'0000'0000'0000'1001'0000)
		ARM_MultiplyLong();
	else if ((m_currentOpcode & 0b0000'1111'1011'0000'0000'1111'1111'0000) == 0b0000'0001'0000'0000'0000'0000'1001'0000)
		ARM_SingleDataSwap();
	else if ((m_currentOpcode & 0b0000'1111'1111'1111'1111'1111'1111'0000) == 0b0000'0001'0010'1111'1111'1111'0001'0000)
		ARM_BranchExchange();
	else if ((m_currentOpcode & 0b0000'1110'0100'0000'0000'1111'1001'0000) == 0b0000'0000'0000'0000'0000'0000'1001'0000)
		ARM_HalfwordTransferRegisterOffset();
	else if ((m_currentOpcode & 0b0000'1110'0100'0000'0000'0000'1001'0000) == 0b0000'0000'0100'0000'0000'0000'1001'0000)
		ARM_HalfwordTransferImmediateOffset();
	else if ((m_currentOpcode & 0b0000'1100'0000'0000'0000'0000'0000'0000) == 0b0000'0000'0000'0000'0000'0000'0000'0000)
		ARM_DataProcessing();
	else if ((m_currentOpcode & 0b0000'1110'0000'0000'0000'0000'0001'0000) == 0b0000'0110'0000'0000'0000'0000'0001'0000)
		ARM_Undefined();
	else if ((m_currentOpcode & 0b0000'1100'0000'0000'0000'0000'0000'0000) == 0b0000'0100'0000'0000'0000'0000'0000'0000)
		ARM_SingleDataTransfer();
	else if ((m_currentOpcode & 0b0000'1110'0000'0000'0000'0000'0000'0000) == 0b0000'1000'0000'0000'0000'0000'0000'0000)
		ARM_BlockDataTransfer();
	else if ((m_currentOpcode & 0b0000'1110'0000'0000'0000'0000'0000'0000) == 0b0000'1100'0000'0000'0000'0000'0000'0000)
		ARM_CoprocessorDataTransfer();
	else if ((m_currentOpcode & 0b0000'1111'0000'0000'0000'0000'0001'0000) == 0b0000'1110'0000'0000'0000'0000'0000'0000)
		ARM_CoprocessorDataOperation();
	else if ((m_currentOpcode & 0b0000'1111'0000'0000'0000'0000'0001'0000) == 0b0000'1110'0000'0000'0000'0000'0001'0000)
		ARM_CoprocessorDataTransfer();
	else if ((m_currentOpcode & 0b0000'1111'0000'0000'0000'0000'0000'0000) == 0b0000'1111'0000'0000'0000'0000'0000'0000)
		ARM_SoftwareInterrupt();
	else
	{
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unimplemented opcode (ARM) {:#x}. PC+8={:#x}", m_currentOpcode, R[15]));
		throw std::runtime_error("Invalid opcode");
	}
}

void ARM7TDMI::executeThumb()
{

	Logger::getInstance()->msg(LoggerSeverity::Info, std::format("Execute opcode (Thumb) {:#x}. PC={:#x}", m_currentOpcode, R[15] - 4));

	if ((m_currentOpcode & 0b1111'1000'0000'0000) == 0b0100'1000'0000'0000)
		Thumb_PCRelativeLoad();
	else
	{
		Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unimplemented opcode (Thumb) {:#x}. PC+4={:#x}", m_currentOpcode, R[15]));
		throw std::runtime_error("Invalid opcode");
	}
}

void ARM7TDMI::flushPipeline()
{
	for (int i = 0; i < 3; i++)
		m_pipeline[i].state = PipelineState::UNFILLED;
	m_pipelinePtr = 0;
	m_shouldFlush = false;
}

bool ARM7TDMI::checkConditions()
{
	uint8_t opFlags = (m_currentOpcode >> 28) & 0xF;
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
	case 9: return (!m_getCarryFlag() || m_getZeroFlag());
	case 10: return ((m_getNegativeFlag() && m_getOverflowFlag()) || (!m_getNegativeFlag() && !m_getOverflowFlag()));
	case 11: return (m_getNegativeFlag() != m_getOverflowFlag());
	case 12: return !m_getZeroFlag() && (m_getNegativeFlag() == m_getOverflowFlag());
	case 13: return m_getZeroFlag() || (m_getNegativeFlag() != m_getOverflowFlag());
	case 14: return true;
	case 15: Logger::getInstance()->msg(LoggerSeverity::Error, "Invalid condition code 1111 !!!!"); break;
	}
	return false;
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
	uint8_t mode = CPSR & 0x1F;
	if (reg < 8)		//R0-R7 not banked so this is gucci
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

void ARM7TDMI::setReg(uint8_t reg, uint32_t value)
{
	uint8_t mode = CPSR & 0x1F;
	if (reg < 8)		//R0-R7 not banked so this is gucci
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
		m_shouldFlush = true;	//modifying PC always causes flush
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
	}
	Logger::getInstance()->msg(LoggerSeverity::Error, "Tried to get SPSR in incorrect mode!!");
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
	default:
		Logger::getInstance()->msg(LoggerSeverity::Error, "Tried to set SPSR in incorrect mode!!");
		break;
	}
}
