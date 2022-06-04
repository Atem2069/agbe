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
	bool checkConditions(uint8_t code);

	//misc flag stuff
	bool m_getNegativeFlag();
	bool m_getZeroFlag();
	bool m_getCarryFlag();
	bool m_getOverflowFlag();

	void m_setNegativeFlag(bool value);
	void m_setZeroFlag(bool value);
	void m_setCarryFlag(bool value);
	void m_setOverflowFlag(bool value);

	//get/set registers
	uint32_t getReg(uint8_t reg, bool forceUser=false);
	void setReg(uint8_t reg, uint32_t value, bool forceUser=false);

	uint32_t getSPSR();
	void setSPSR(uint32_t value);

	//ARM instruction set
	void ARM_Branch();
	void ARM_DataProcessing();
	void ARM_PSRTransfer();
	void ARM_Multiply();
	void ARM_MultiplyLong();
	void ARM_SingleDataSwap();
	void ARM_BranchExchange();
	void ARM_HalfwordTransferRegisterOffset();
	void ARM_HalfwordTransferImmediateOffset();
	void ARM_SingleDataTransfer();
	void ARM_Undefined();
	void ARM_BlockDataTransfer();
	void ARM_CoprocessorDataTransfer();
	void ARM_CoprocessorDataOperation();
	void ARM_CoprocessorRegisterTransfer();
	void ARM_SoftwareInterrupt();

	//Thumb instruction set
	void Thumb_MoveShiftedRegister();
	void Thumb_AddSubtract();
	void Thumb_MoveCompareAddSubtractImm();
	void Thumb_ALUOperations();
	void Thumb_HiRegisterOperations();
	void Thumb_PCRelativeLoad();
	void Thumb_LoadStoreRegisterOffset();
	void Thumb_LoadStoreSignExtended();
	void Thumb_LoadStoreImmediateOffset();
	void Thumb_LoadStoreHalfword();
	void Thumb_SPRelativeLoadStore();
	void Thumb_LoadAddress();
	void Thumb_AddOffsetToStackPointer();
	void Thumb_PushPopRegisters();
	void Thumb_MultipleLoadStore();
	void Thumb_ConditionalBranch();
	void Thumb_SoftwareInterrupt();
	void Thumb_UnconditionalBranch();
	void Thumb_LongBranchWithLink();

	//Barrel shifter ops
	uint32_t LSL(uint32_t val, int shiftAmount, int& carry);
	uint32_t LSR(uint32_t val, int shiftAmount, int& carry);
	uint32_t ASR(uint32_t val, int shiftAmount, int& carry);
	uint32_t ROR(uint32_t val, int shiftAmount, int& carry);
	uint32_t RORSpecial(uint32_t val, int shiftAmount, int& carry);

	//Flag setting
	void setLogicalFlags(uint32_t result, int carry);
	void setArithmeticFlags(uint32_t input, uint32_t operand, uint32_t result, bool addition);
};