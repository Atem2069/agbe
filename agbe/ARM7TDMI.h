#pragma once

#include"Logger.h"
#include"Bus.h"

#include<iostream>
#include<stdexcept>

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
	uint32_t R8_fiq=0, R9_fiq=0, R10_fiq=0, R11_fiq=0, R12_fiq=0, R13_fiq=0, R14_fiq=0;	//additional banked registers in FIQ mode
	uint32_t R13_svc=0, R14_svc=0;												//SVC mode banked registers
	uint32_t R13_abt=0, R14_abt=0;												//ABT mode banked registers (is this mode even used? GBA doesn't do data aborts..)
	uint32_t R13_irq=0, R14_irq=0;												//IRQ mode banked registers
	uint32_t R13_und=0, R14_und=0;												//UND(undefined) mode banked registers - trap for when CPU executes invalid opcode

	uint32_t CPSR=0;
	uint32_t SPSR_fiq=0, SPSR_svc=0, SPSR_abt=0, SPSR_irq=0, SPSR_und=0;

	void fetch();
	void execute();
	void flushPipeline();

	void executeThumb();

	uint32_t m_currentOpcode = 0;

	//checking conditions for ARM opcodes
	bool checkConditions();

	//misc flag stuff
	bool m_getNegativeFlag();
	bool m_getZeroFlag();
	bool m_getCarryFlag();
	bool m_getOverflowFlag();

	void m_setNegativeFlag(bool value);
	void m_setZeroFlag(bool value);
	void m_setCarryFlag(bool value);
	void m_setOverflowFlag(bool value);

};