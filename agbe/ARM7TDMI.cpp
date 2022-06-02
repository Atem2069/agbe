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
	else
		m_pipelinePtr = ((m_pipelinePtr + 1) % 3);
}

void ARM7TDMI::fetch()
{
	bool thumb = (CPSR >> 5) & 0b1;
	int curPipelinePtr = m_pipelinePtr;
	m_pipeline[curPipelinePtr].state = PipelineState::FILLED;
	if (thumb)
	{
		m_pipeline[curPipelinePtr].opcode = m_bus->read16(R[15]);
		R[15] += 2;
	}
	else
	{
		m_pipeline[curPipelinePtr].opcode = m_bus->read32(R[15]);
		R[15] += 4;
	}
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

	//decode instruction here

	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unimplemented opcode (ARM) {:#x}. PC+12={:#x}", m_currentOpcode, R[15]));
	throw std::runtime_error("Invalid opcode");
}

void ARM7TDMI::executeThumb()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unimplemented opcode (Thumb) {:#x}. PC+12={:#x}", m_currentOpcode, R[15]));
	throw std::runtime_error("Invalid opcode");
}

void ARM7TDMI::flushPipeline()
{
	for (int i = 0; i < 3; i++)
		m_pipeline[i].state = PipelineState::UNFILLED;
	m_pipelinePtr = 0;
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
	case 12: return !m_getZeroFlag && (m_getNegativeFlag() == m_getOverflowFlag());
	case 13: return m_getZeroFlag || (m_getNegativeFlag() != m_getOverflowFlag());
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