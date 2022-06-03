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
	Logger::getInstance()->msg(LoggerSeverity::Error, std::format("Unimplemented opcode (Thumb) {:#x}. PC+8={:#x}", m_currentOpcode, R[15]));
	throw std::runtime_error("Invalid opcode");
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

void ARM7TDMI::ARM_Branch()
{
	bool link = ((m_currentOpcode >> 24) & 0b1);
	int32_t offset = m_currentOpcode & 0x00FFFFFF;
	offset <<= 2;	//now 26 bits
	if ((offset >> 25) & 0b1)	//if sign (bit 25) set, then must sign extend
		offset |= 0xFC000000;
	
	//set R15
	setReg(15, getReg(15) + offset);
	if (link)
		setReg(14, getReg(15) - 4);	//to accommodate for pipeline - the effective value saved in LR is 4 bytes ahead of the opcode

}

void ARM7TDMI::ARM_DataProcessing()
{
	//check if psr transfer instead (this is dumb but can probs be refactored completely)
	if ((m_currentOpcode & 0b0000'1111'1011'1111'0000'1111'1111'1111) == 0b0000'0001'0000'1111'0000'0000'0000'0000)
	{
		ARM_PSRTransfer();
		return;
	}
	if ((m_currentOpcode & 0b0000'1111'1011'1111'1111'1111'1111'0000) == 0b0000'0001'0010'1001'1111'0000'0000'0000)
	{
		ARM_PSRTransfer();
		return;
	}
	if ((m_currentOpcode & 0b0000'1101'1011'1111'1111'0000'0000'0000) == 0b0000'0001'0010'1000'1111'0000'0000'0000)
	{
		ARM_PSRTransfer();
		return;
	}

	bool immediate = ((m_currentOpcode >> 25) & 0b1);
	uint8_t operation = ((m_currentOpcode >> 21) & 0xF);
	bool setConditionCodes = ((m_currentOpcode >> 20) & 0b1);
	uint8_t op1Idx = ((m_currentOpcode >> 16) & 0xF);
	uint8_t destRegIdx = ((m_currentOpcode >> 12) & 0xF);

	bool setCPSR = false;
	if (destRegIdx == 15 && setConditionCodes)	//S=1,Rd=15 - copy SPSR over to CPSR
	{
		setCPSR = true;
		setConditionCodes = false;
	}


	uint32_t operand1 = getReg(op1Idx);
	uint32_t operand2 = 0;
	int shiftCarryOut = -1;	

	//resolve operand 2
	if (immediate) //operand 2 is immediate
	{
		operand2 = m_currentOpcode & 0xFF;
		int shiftAmount = ((m_currentOpcode >> 8) & 0xF) * 2;
		if (shiftAmount > 0)
			operand2 = std::rotr(operand2, shiftAmount);	//afaik immediate shifts don't affect flags
	}

	else		//operand 2 is a register
	{
		uint8_t op2Idx = m_currentOpcode & 0xF;
		operand2 = getReg(op2Idx);
		bool shiftIsRegister = ((m_currentOpcode >> 4) & 0b1);	//bit 4 specifies whether the amount to shift is a register or immediate
		int shiftAmount = 0;
		if (shiftIsRegister)
		{
			if (op2Idx == 15)	//account for R15 being 12 bytes ahead if register-specified shift amount
				operand2 += 4;
			uint8_t shiftRegIdx = (m_currentOpcode >> 8) & 0xF;
			shiftAmount = getReg(shiftRegIdx) & 0xFF;	//only bottom byte used
			shiftCarryOut = m_getCarryFlag();			//just in case we don't shift (if register contains 0)
		}
		else
			shiftAmount = ((m_currentOpcode >> 7) & 0x1F);	//5 bit value (bits 7-11)

		uint8_t shiftType = ((m_currentOpcode >> 5) & 0b11);

		//(register specified shift) - If this byte is zero, the unchanged contents of Rm will be used - and the old value of the CPSR C flag will be passed on
		//(instruction specified shift) - Probs alright to just do a shift by 0, see what happens
		if ((shiftIsRegister && shiftAmount > 0) || (!shiftIsRegister))	//if imm shift, just go for it. if register shift, then only if shift amount > 0
		{
			switch (shiftType)
			{
			case 0: operand2 = LSL(operand2, shiftAmount, shiftCarryOut); break;
			case 1: operand2 = LSR(operand2, shiftAmount, shiftCarryOut); break;
			case 2: operand2 = ASR(operand2, shiftAmount, shiftCarryOut); break;
			case 3: operand2 = ROR(operand2, shiftAmount, shiftCarryOut); break;
			}
		}
	}

	uint32_t result = 0;
	uint32_t carryIn = m_getCarryFlag() & 0b1;
	bool shouldFlush = true;	//for setting ARM/THUMB state if R15 is affected (seems only CMP can unset)
	switch (operation)
	{
	case 0:	//AND
		result = operand1 & operand2;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setLogicalFlags(result, shiftCarryOut); }
		break;
	case 1:	//EOR
		result = operand1 ^ operand2;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setLogicalFlags(result, shiftCarryOut); }
		break;
	case 2:	//SUB
		result = operand1 - operand2;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setArithmeticFlags(operand1, operand2, result, false); }
		break;
	case 3:	//RSB
		result = operand2 - operand1;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setArithmeticFlags(operand2, operand1, result, false); }
		break;
	case 4:	//ADD
		result = operand1 + operand2;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setArithmeticFlags(operand1, operand2, result, true); }
		break;
	case 5:	//ADC
		result = operand1 + operand2 + carryIn;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setArithmeticFlags(operand1, operand2 + carryIn, result, true); }
		break;
	case 6:	//SBC
		result = operand1 - operand2 + carryIn - 1;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setArithmeticFlags(operand1, operand2 + carryIn + 1, result, false); }
		break;
	case 7:	//RSC
		result = operand2 - operand1 + carryIn - 1;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setArithmeticFlags(operand2, operand1 + carryIn + 1, result, false); }
		break;
	case 8:	//TST
		result = operand1 & operand2;
		setLogicalFlags(result, shiftCarryOut);
		break;
	case 9:	//TEQ
		result = operand1 ^ operand2;
		setLogicalFlags(result, shiftCarryOut);
		break;
	case 10: //CMP
		result = operand1 - operand2;
		setArithmeticFlags(operand1, operand2, result, false);
		shouldFlush = false;
		break;
	case 11: //CMN
		result = operand1 + operand2;
		setArithmeticFlags(operand1, operand2, result, true);
		break;
	case 12: //ORR
		result = operand1 | operand2;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setLogicalFlags(result, shiftCarryOut); }
		break;
	case 13: //MOV
		result = operand2;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setLogicalFlags(result, shiftCarryOut); }
		break;
	case 14:	//BIC
		result = operand1 & (~operand2);
		setReg(destRegIdx, result);
		if (setConditionCodes) { setLogicalFlags(result, shiftCarryOut); }
		break;
	case 15:	//MVN
		result = ~operand2;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setLogicalFlags(result, shiftCarryOut); }
		break;
	}

	if (destRegIdx == 15)
	{
		Logger::getInstance()->msg(LoggerSeverity::Info, "Dest reg was 15!!");
		if (setCPSR)
		{
			Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented CPSR set (Rd=15)");
		}
		if (shouldFlush == true)
		{
			if ((R[15] & 0b1) || ((CPSR >> 5) & 0b1))	//enter thumb
			{
				CPSR |= 0x20;	//set THUMB bit
				setReg(15, getReg(15) & ~0b1);	//clear bit 0
			}
			else
				setReg(15, getReg(15) & ~0b11);	//clear bits 0,1
		}
	}
}

void ARM7TDMI::ARM_PSRTransfer()
{

	uint32_t opBits = ((m_currentOpcode >> 12) & 0x3FF);	//extract 10 bits identifying opcode

	if (opBits == 0b1010011111)
	{
		bool modifySPSR = ((m_currentOpcode >> 22) & 0b1);
		uint8_t srcRegIdx = m_currentOpcode & 0xF;
		uint32_t newPSRData = getReg(srcRegIdx);
		if (modifySPSR)
			setSPSR(newPSRData);
		else
		{
			//todo: check if unprivileged mode
			CPSR = newPSRData;
		}
	}
	else if (opBits == 0b1010001111)
	{
		Logger::getInstance()->msg(LoggerSeverity::Info, "MSR (flags only)");
		throw std::runtime_error("unimplemented");
	}
	else
	{
		Logger::getInstance()->msg(LoggerSeverity::Info, "MRS");
		throw std::runtime_error("unimplemented");
	}
}

void ARM7TDMI::ARM_Multiply()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_MultiplyLong()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_SingleDataSwap()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_BranchExchange()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_HalfwordTransferRegisterOffset()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_HalfwordTransferImmediateOffset()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_SingleDataTransfer()
{
	bool immediate = ((m_currentOpcode >> 25) & 0b1);
	bool preIndex = ((m_currentOpcode >> 24) & 0b1);
	bool upDown = ((m_currentOpcode >> 23) & 0b1);	//1=up,0=down
	bool byteWord = ((m_currentOpcode >> 22) & 0b1);//1=byte,0=word
	bool writeback = ((m_currentOpcode >> 21) & 0b1);
	bool loadStore = ((m_currentOpcode >> 20) & 0b1);

	uint8_t baseRegIdx = ((m_currentOpcode >> 16) & 0xF);
	uint8_t destRegIdx = ((m_currentOpcode >> 12) & 0xF);

	uint32_t base = getReg(baseRegIdx);

	int32_t offset = 0;
	//resolve offset
	if (!immediate)	//I=0 means immediate!!
	{
		offset = m_currentOpcode & 0xFFF;	//extract 12-bit imm offset
	}
	else
	{
		uint8_t offsetRegIdx = m_currentOpcode & 0xF;
		offset = getReg(offsetRegIdx);


		//register specified shifts not available
		uint8_t shiftAmount = ((m_currentOpcode >> 7) & 0x1F);	//5 bit shift amount
		uint8_t shiftType = ((m_currentOpcode >> 5) & 0b11);
		if (((m_currentOpcode >> 4) & 0b1) == 1)
			Logger::getInstance()->msg(LoggerSeverity::Error, "Opcode encoding is not valid! bit 4 shouldn't be set!!");

		int garbageCarry = 0;
		switch (shiftType)
		{
		case 0: offset = LSL(offset, shiftAmount, garbageCarry); break;
		case 1: offset = LSR(offset, shiftAmount, garbageCarry); break;
		case 2: offset = ASR(offset, shiftAmount, garbageCarry); break;
		case 3: offset = ROR(offset, shiftAmount, garbageCarry); break;
		}

	}

	//offset now resolved
	if (!upDown)	//offset subtracted if up/down bit is 0
		offset = -offset;

	if (preIndex)
		base += offset;

	if (loadStore) //load value
	{
		uint32_t val = 0;
		if (byteWord)
		{
			val = m_bus->read8(base);
		}
		else
		{
			//todo: account for unaligned reads
			val = m_bus->read32(base);
		}
		setReg(destRegIdx, val);
	}
	else //store value
	{
		uint32_t val = getReg(destRegIdx);
		if (destRegIdx == 15)	//R15 12 bytes ahead (instead of 8) if used for store
			val += 4;
		if (byteWord)
		{
			m_bus->write8(base,val & 0xFF);
		}
		else
		{
			m_bus->write32(base, val);
		}
	}

	if (!preIndex)
		base += offset;

	if (writeback || !preIndex)	//post index implies writeback
	{
		setReg(baseRegIdx, base);
	}
}

void ARM7TDMI::ARM_Undefined()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_BlockDataTransfer()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_CoprocessorDataTransfer()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_CoprocessorDataOperation()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_CoprocessorRegisterTransfer()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_SoftwareInterrupt()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}