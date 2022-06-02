#pragma once

#include"Logger.h"
#include"Bus.h"

enum class PipelineState
{
	UNFILLED,
	FILLED
};

struct Pipeline
{
	PipelineState state;
	uint32_t opcode;
};

class ARM7TDMI
{
public:
	ARM7TDMI(std::shared_ptr<Bus> bus);
	~ARM7TDMI();

	void step();
private:
	std::shared_ptr<Bus> m_bus;

	Pipeline m_pipeline[3];
	uint8_t m_pipelinePtr = 0;
	bool m_shouldFlush = false;

	uint32_t R[16];	//general registers - and default registers in usermode
	uint32_t R8_fiq, R9_fiq, R10_fiq, R11_fiq, R12_fiq, R13_fiq, R14_fiq;	//additional banked registers in FIQ mode
	uint32_t R13_svc, R14_svc;												//SVC mode banked registers
	uint32_t R13_abt, R14_abt;												//ABT mode banked registers (is this mode even used? GBA doesn't do data aborts..)
	uint32_t R13_irq, R14_irq;												//IRQ mode banked registers
	uint32_t R13_und, R14_und;												//UND(undefined) mode banked registers - trap for when CPU executes invalid opcode

	uint32_t CPSR;
	uint32_t SPSR_fiq, SPSR_svc, SPSR_abt, SPSR_irq, SPSR_und;

	void fetch();
	void execute();
	void flushPipeline();

};