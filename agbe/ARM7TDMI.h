#pragma once

#include"Logger.h"
#include"Bus.h"
#include"InterruptManager.h"
#include"Scheduler.h"

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
	ARM7TDMI(std::shared_ptr<Bus> bus, std::shared_ptr<InterruptManager> interruptManager, std::shared_ptr<Scheduler> scheduler);
	~ARM7TDMI();

	void step();
private:
	std::shared_ptr<Bus> m_bus;
	std::shared_ptr<InterruptManager> m_interruptManager;
	std::shared_ptr<Scheduler> m_scheduler;

	Pipeline m_pipeline[3];
	uint8_t m_pipelinePtr = 0;
	bool pipelineFull = false;
	bool m_pipelineFlushed = false;
	bool nextFetchNonsequential = true;

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

	bool dispatchInterrupt();

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

	int calculateMultiplyCycles(uint32_t operand, bool isSigned);

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

	typedef void(ARM7TDMI::*instructionFn)();

	//Barrel shifter ops
	uint32_t LSL(uint32_t val, int shiftAmount, int& carry);
	uint32_t LSR(uint32_t val, int shiftAmount, int& carry);
	uint32_t ASR(uint32_t val, int shiftAmount, int& carry);
	uint32_t ROR(uint32_t val, int shiftAmount, int& carry);
	uint32_t RORSpecial(uint32_t val, int shiftAmount, int& carry);

	//Flag setting
	void setLogicalFlags(uint32_t result, int carry);
	void setArithmeticFlags(uint32_t input, uint64_t operand, uint32_t result, bool addition);

	static constexpr auto thumbTable = []
	{
		constexpr auto lutSize = 1024;
		std::array<instructionFn, lutSize> tempLut = {};
		uint32_t tempOpcode = 0;
		for (uint32_t i = 0; i < 1024; i++)
		{
			tempOpcode = (i << 6);
			if ((tempOpcode & 0b1111'1000'0000'0000) == 0b0001'1000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_AddSubtract;
			else if ((tempOpcode & 0b1110'0000'0000'0000) == 0b0000'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_MoveShiftedRegister;
			else if ((tempOpcode & 0b1110'0000'0000'0000) == 0b0010'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_MoveCompareAddSubtractImm;
			else if ((tempOpcode & 0b1111'1100'0000'0000) == 0b0100'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_ALUOperations;
			else if ((tempOpcode & 0b1111'1100'0000'0000) == 0b0100'0100'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_HiRegisterOperations;
			else if ((tempOpcode & 0b1111'1000'0000'0000) == 0b0100'1000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_PCRelativeLoad;
			else if ((tempOpcode & 0b1111'0010'0000'0000) == 0b0101'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_LoadStoreRegisterOffset;
			else if ((tempOpcode & 0b1111'0010'0000'0000) == 0b0101'0010'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_LoadStoreSignExtended;
			else if ((tempOpcode & 0b1110'0000'0000'0000) == 0b0110'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_LoadStoreImmediateOffset;
			else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1000'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_LoadStoreHalfword;
			else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1001'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_SPRelativeLoadStore;
			else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1010'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_LoadAddress;
			else if ((tempOpcode & 0b1111'1111'0000'0000) == 0b1011'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_AddOffsetToStackPointer;
			else if ((tempOpcode & 0b1111'0110'0000'0000) == 0b1011'0100'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_PushPopRegisters;
			else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1100'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_MultipleLoadStore;
			else if ((tempOpcode & 0b1111'1111'0000'0000) == 0b1101'1111'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_SoftwareInterrupt;
			else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1101'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_ConditionalBranch;
			else if ((tempOpcode & 0b1111'1000'0000'0000) == 0b1110'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_UnconditionalBranch;
			else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1111'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::Thumb_LongBranchWithLink;
			else
			{
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_Undefined;	//meh. good enough?
			}
		}
		return tempLut;
	}();
	static constexpr auto armTable = []
	{
		constexpr auto lutSize = 4096;
		std::array<instructionFn, lutSize> tempLut = {};
		uint32_t tempOpcode = 0;
		for (uint32_t i = 0; i < 4096; i++)
		{
			tempOpcode = ((i & 0xFF0) << 16) | ((i & 0xF) << 4);	//expand instruction so bits 20-27 contain top 8 bits of i, bits 4-7 contain lower 4 bits
			if ((tempOpcode & 0b0000'1110'0000'0000'0000'0000'0000'0000) == 0b0000'1010'0000'0000'0000'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_Branch;
			else if ((tempOpcode & 0b0000'1111'1100'0000'0000'0000'1111'0000) == 0b0000'0000'0000'0000'0000'0000'1001'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_Multiply;
			else if ((tempOpcode & 0b0000'1111'1000'0000'0000'0000'1111'0000) == 0b0000'0000'1000'0000'0000'0000'1001'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_MultiplyLong;
			else if ((tempOpcode & 0b0000'1111'1011'0000'0000'1111'1111'0000) == 0b0000'0001'0000'0000'0000'0000'1001'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_SingleDataSwap;
			else if ((tempOpcode & 0b0000'1110'0100'0000'0000'1111'1001'0000) == 0b0000'0000'0000'0000'0000'0000'1001'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_HalfwordTransferRegisterOffset;
			else if ((tempOpcode & 0b0000'1110'0100'0000'0000'0000'1001'0000) == 0b0000'0000'0100'0000'0000'0000'1001'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_HalfwordTransferImmediateOffset;
			else if ((tempOpcode & 0b0000'1111'1111'0000'0000'0000'1111'0000) == 0b0000'0001'0010'0000'0000'0000'0001'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_BranchExchange;
			else if ((tempOpcode & 0b0000'1100'0000'0000'0000'0000'0000'0000) == 0b0000'0000'0000'0000'0000'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_DataProcessing;
			else if ((tempOpcode & 0b0000'1110'0000'0000'0000'0000'0001'0000) == 0b0000'0110'0000'0000'0000'0000'0001'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_Undefined;
			else if ((tempOpcode & 0b0000'1100'0000'0000'0000'0000'0000'0000) == 0b0000'0100'0000'0000'0000'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_SingleDataTransfer;
			else if ((tempOpcode & 0b0000'1110'0000'0000'0000'0000'0000'0000) == 0b0000'1000'0000'0000'0000'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_BlockDataTransfer;
			else if ((tempOpcode & 0b0000'1110'0000'0000'0000'0000'0000'0000) == 0b0000'1100'0000'0000'0000'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_CoprocessorDataTransfer;
			else if ((tempOpcode & 0b0000'1111'0000'0000'0000'0000'0001'0000) == 0b0000'1110'0000'0000'0000'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_CoprocessorDataOperation;
			else if ((tempOpcode & 0b0000'1111'0000'0000'0000'0000'0001'0000) == 0b0000'1110'0000'0000'0000'0000'0001'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_CoprocessorDataTransfer;
			else if ((tempOpcode & 0b0000'1111'0000'0000'0000'0000'0000'0000) == 0b0000'1111'0000'0000'0000'0000'0000'0000)
				tempLut[i] = (instructionFn)&ARM7TDMI::ARM_SoftwareInterrupt;
		}
		return tempLut;
	}();

};