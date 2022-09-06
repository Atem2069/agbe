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
	static constexpr int incrAmountLUT[2] = { 4,2 };
	std::shared_ptr<Bus> m_bus;
	std::shared_ptr<InterruptManager> m_interruptManager;
	std::shared_ptr<Scheduler> m_scheduler;

	Pipeline m_pipeline[3];
	uint8_t m_pipelinePtr = 0;
	bool pipelineFull = false;
	bool m_pipelineFlushed = false;
	bool nextFetchNonsequential = true;

	uint32_t R[16];
	uint32_t usrBankedRegisters[2];          //user mode r13-r14
	uint32_t usrExtraBankedRegisters[5];	 //not really only usermode, but banks of r8-r12 for if fiq mode is used
	uint32_t svcBankedRegisters[2];			 //supervisor r13-r14
	uint32_t abtBankedRegisters[2];          //abort r13-r14
	uint32_t irqBankedRegisters[2];          //irq r13-r14
	uint32_t undBankedRegisters[2];          //undefined mode r13-r14
	uint32_t fiqBankedRegisters[2];          //fiq r13-r14
	uint32_t fiqExtraBankedRegisters[5];     //fiq r8-r12 (extra regs)

	uint32_t CPSR=0;
	uint32_t SPSR_fiq=0, SPSR_svc=0, SPSR_abt=0, SPSR_irq=0, SPSR_und=0;

	bool m_inThumbMode = false;

	void fetch();
	void execute();
	void flushPipeline();
	void refillPipeline();

	void executeThumb();

	bool dispatchInterrupt();

	uint32_t m_currentOpcode = 0;

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
	uint32_t getReg(uint8_t reg);
	void setReg(uint8_t reg, uint32_t value);

	uint8_t m_lastCheckModeBits = 0;
	void swapBankedRegisters();

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

	//magic code for generating compile time arm/thumb luts

	template<int i, int max> static consteval void setARMTableEntries(auto& table)
	{
		uint32_t tempOpcode = ((i & 0xFF0) << 16) | ((i & 0xF) << 4);	//expand instruction so bits 20-27 contain top 8 bits of i, bits 4-7 contain lower 4 bits
		if ((tempOpcode & 0b0000'1110'0000'0000'0000'0000'0000'0000) == 0b0000'1010'0000'0000'0000'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_Branch;
		else if ((tempOpcode & 0b0000'1111'1100'0000'0000'0000'1111'0000) == 0b0000'0000'0000'0000'0000'0000'1001'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_Multiply;
		else if ((tempOpcode & 0b0000'1111'1000'0000'0000'0000'1111'0000) == 0b0000'0000'1000'0000'0000'0000'1001'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_MultiplyLong;
		else if ((tempOpcode & 0b0000'1111'1011'0000'0000'1111'1111'0000) == 0b0000'0001'0000'0000'0000'0000'1001'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_SingleDataSwap;
		else if ((tempOpcode & 0b0000'1110'0100'0000'0000'1111'1001'0000) == 0b0000'0000'0000'0000'0000'0000'1001'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_HalfwordTransferRegisterOffset;
		else if ((tempOpcode & 0b0000'1110'0100'0000'0000'0000'1001'0000) == 0b0000'0000'0100'0000'0000'0000'1001'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_HalfwordTransferImmediateOffset;
		else if ((tempOpcode & 0b0000'1111'1111'0000'0000'0000'1111'0000) == 0b0000'0001'0010'0000'0000'0000'0001'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_BranchExchange;
		else if ((tempOpcode & 0b0000'1100'0000'0000'0000'0000'0000'0000) == 0b0000'0000'0000'0000'0000'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_DataProcessing;
		else if ((tempOpcode & 0b0000'1110'0000'0000'0000'0000'0001'0000) == 0b0000'0110'0000'0000'0000'0000'0001'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_Undefined;
		else if ((tempOpcode & 0b0000'1100'0000'0000'0000'0000'0000'0000) == 0b0000'0100'0000'0000'0000'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_SingleDataTransfer;
		else if ((tempOpcode & 0b0000'1110'0000'0000'0000'0000'0000'0000) == 0b0000'1000'0000'0000'0000'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_BlockDataTransfer;
		else if ((tempOpcode & 0b0000'1110'0000'0000'0000'0000'0000'0000) == 0b0000'1100'0000'0000'0000'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_CoprocessorDataTransfer;
		else if ((tempOpcode & 0b0000'1111'0000'0000'0000'0000'0001'0000) == 0b0000'1110'0000'0000'0000'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_CoprocessorDataOperation;
		else if ((tempOpcode & 0b0000'1111'0000'0000'0000'0000'0001'0000) == 0b0000'1110'0000'0000'0000'0000'0001'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_CoprocessorDataTransfer;
		else if ((tempOpcode & 0b0000'1111'0000'0000'0000'0000'0000'0000) == 0b0000'1111'0000'0000'0000'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::ARM_SoftwareInterrupt;

		if constexpr ((i + 1) < max)
			setARMTableEntries<i + 1, max>(table);
	}

	template<int i, int max> static consteval void setThumbTableEntries(auto& table)
	{
		uint16_t tempOpcode = (i << 6);
		if ((tempOpcode & 0b1111'1000'0000'0000) == 0b0001'1000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_AddSubtract;
		else if ((tempOpcode & 0b1110'0000'0000'0000) == 0b0000'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_MoveShiftedRegister;
		else if ((tempOpcode & 0b1110'0000'0000'0000) == 0b0010'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_MoveCompareAddSubtractImm;
		else if ((tempOpcode & 0b1111'1100'0000'0000) == 0b0100'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_ALUOperations;
		else if ((tempOpcode & 0b1111'1100'0000'0000) == 0b0100'0100'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_HiRegisterOperations;
		else if ((tempOpcode & 0b1111'1000'0000'0000) == 0b0100'1000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_PCRelativeLoad;
		else if ((tempOpcode & 0b1111'0010'0000'0000) == 0b0101'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_LoadStoreRegisterOffset;
		else if ((tempOpcode & 0b1111'0010'0000'0000) == 0b0101'0010'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_LoadStoreSignExtended;
		else if ((tempOpcode & 0b1110'0000'0000'0000) == 0b0110'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_LoadStoreImmediateOffset;
		else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1000'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_LoadStoreHalfword;
		else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1001'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_SPRelativeLoadStore;
		else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1010'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_LoadAddress;
		else if ((tempOpcode & 0b1111'1111'0000'0000) == 0b1011'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_AddOffsetToStackPointer;
		else if ((tempOpcode & 0b1111'0110'0000'0000) == 0b1011'0100'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_PushPopRegisters;
		else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1100'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_MultipleLoadStore;
		else if ((tempOpcode & 0b1111'1111'0000'0000) == 0b1101'1111'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_SoftwareInterrupt;
		else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1101'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_ConditionalBranch;
		else if ((tempOpcode & 0b1111'1000'0000'0000) == 0b1110'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_UnconditionalBranch;
		else if ((tempOpcode & 0b1111'0000'0000'0000) == 0b1111'0000'0000'0000)
			table[i] = (instructionFn)&ARM7TDMI::Thumb_LongBranchWithLink;
		else
		{
			table[i] = (instructionFn)&ARM7TDMI::ARM_Undefined;	//meh. good enough?
		}

		if constexpr ((i + 1) < max)
			setThumbTableEntries<i+1, max>(table);
	}

	static consteval std::array<instructionFn, 4096> genARMTable();
	static consteval std::array<instructionFn, 1024> genThumbTable();

	//messy.. generates 16x16 LUT covering all combinations of CPSR flags and condition codes (reduces extra call at runtime)
	static consteval std::array<uint16_t, 16> genConditionCodeTable()
	{
		std::array<uint16_t, 16> conditionCodeLUT;
		for (int i = 0; i < 16; i++)		//going through all possible CPSR condition combinations
		{
			//extract flags 
			bool N = (i >> 3) & 0b1;
			bool Z = (i >> 2) & 0b1;
			bool C = (i >> 1) & 0b1;
			bool V = i & 0b1;

			//set flags for all 16 possible condition codes
			conditionCodeLUT[i] = Z;
			conditionCodeLUT[i] |= (!Z) << 1;
			conditionCodeLUT[i] |= (C) << 2;
			conditionCodeLUT[i] |= (!C) << 3;
			conditionCodeLUT[i] |= (N) << 4;
			conditionCodeLUT[i] |= (!N) << 5;
			conditionCodeLUT[i] |= (V) <<  6;
			conditionCodeLUT[i] |= (!V) << 7;
			conditionCodeLUT[i] |= (C && !Z) << 8;
			conditionCodeLUT[i] |= (!C || Z) << 9;
			conditionCodeLUT[i] |= (N == V) << 10;
			conditionCodeLUT[i] |= (N != V) << 11;
			conditionCodeLUT[i] |= ((!Z) && (N == V)) << 12;
			conditionCodeLUT[i] |= (Z || (N != V)) << 13;
			conditionCodeLUT[i] |= (1 << 14);
			conditionCodeLUT[i] |= (1 << 15);	//think this should be right

		}

		return conditionCodeLUT;
	}
};
