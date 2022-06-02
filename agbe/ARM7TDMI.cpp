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
	bool thumb = (CPSR >> 6) & 0b1;
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

	uint32_t curOpcode = m_pipeline[curPipelinePtr].opcode;
	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unimplemented opcode {:#x}. PC+12={:#x} Is this THUMB? - {}", curOpcode, R[15], (CPSR >> 6) & 0b1));
	throw std::runtime_error("Invalid opcode");
}

void ARM7TDMI::flushPipeline()
{
	for (int i = 0; i < 3; i++)
		m_pipeline[i].state = PipelineState::UNFILLED;
	m_pipelinePtr = 0;
}