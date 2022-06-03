#include"ARM7TDMI.h"


//start of Thumb instruction set
void ARM7TDMI::Thumb_MoveShiftedRegister()
{
	uint8_t operation = ((m_currentOpcode >> 11) & 0b11);
	uint8_t shiftAmount = ((m_currentOpcode >> 6) & 0b11111);
	uint8_t srcRegIdx = ((m_currentOpcode >> 3) & 0b111);
	uint8_t destRegIdx = m_currentOpcode & 0b111;

	uint32_t srcVal = getReg(srcRegIdx);
	uint32_t result = 0;
	int carry = -1;
	switch (operation)
	{
	case 0: result=LSL(srcVal, shiftAmount, carry); break;
	case 1: result=LSR(srcVal, shiftAmount, carry); break;
	case 2: result=ASR(srcVal, shiftAmount, carry); break;
	}

	setLogicalFlags(result, carry);
	setReg(destRegIdx, result);
}

void ARM7TDMI::Thumb_AddSubtract()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_MoveCompareAddSubtractImm()
{
	uint32_t offset = m_currentOpcode & 0xFF;
	uint8_t srcDestRegIdx = ((m_currentOpcode >> 8) & 0b111);
	uint8_t operation = ((m_currentOpcode >> 11) & 0b11);

	uint32_t operand1 = getReg(srcDestRegIdx);
	uint32_t result = 0;

	switch (operation)
	{
	case 0:
		result = offset;
		setReg(srcDestRegIdx, result);
		setLogicalFlags(result, -1);
		break;
	case 1:
		result = operand1 - offset;
		setArithmeticFlags(operand1, offset, result, false);
		break;
	case 2:
		result = operand1 + offset;
		setReg(srcDestRegIdx, result);
		setArithmeticFlags(operand1, offset, result, true);
		break;
	case 3:
		result = operand1 - offset;
		setReg(srcDestRegIdx, result);
		setArithmeticFlags(operand1, offset, result, false);
		break;
	}
}

void ARM7TDMI::Thumb_ALUOperations()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_HiRegisterOperations()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_PCRelativeLoad()
{
	uint32_t offset = (m_currentOpcode & 0xFF) << 2;
	uint32_t PC = getReg(15) & ~0b11;	//PC is force aligned to word boundary

	uint8_t destRegIdx = ((m_currentOpcode >> 8) & 0b111);

	uint32_t val = m_bus->read32(PC + offset);
	setReg(destRegIdx, val);
}

void ARM7TDMI::Thumb_LoadStoreRegisterOffset()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_LoadStoreSignExtended()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_LoadStoreImmediateOffset()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_LoadStoreHalfword()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_SPRelativeLoadStore()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_LoadAddress()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_AddOffsetToStackPointer()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_PushPopRegisters()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_MultipleLoadStore()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_ConditionalBranch()
{
	uint32_t offset = m_currentOpcode & 0xFF;
	offset <<= 1;
	if (((offset >> 8) & 0b1))	//sign extend (FFFFFF00 shouldn't matter bc bit 8 should be a 1 anyway)
		offset |= 0xFFFFFF00;

	uint8_t condition = ((m_currentOpcode >> 8) & 0xF);
	if (condition == 14 || condition == 15)
		Logger::getInstance()->msg(LoggerSeverity::Error, "Invalid condition code - opcode decoding is likely wrong!!");
	if (!checkConditions(condition))
		return;

	setReg(15, getReg(15) + offset);
}

void ARM7TDMI::Thumb_SoftwareInterrupt()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_UnconditionalBranch()
{
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::Thumb_LongBranchWithLink()
{
	bool highLow = ((m_currentOpcode >> 11) & 0b1);
	uint32_t offset = m_currentOpcode & 0b11111111111;
	if (!highLow)	//H=0: leftshift offset by 12 and add to PC, then store in LR
	{
		offset <<= 12;
		uint32_t res = getReg(15) + offset;
		setReg(14, res);
	}
	else			//H=1: leftshift by 1 and add to LR - then copy LR to PC. copy old PC (-2) to LR and set bit 0
	{
		offset <<= 1;
		uint32_t LR = getReg(14);
		LR += offset;
		setReg(14, ((getReg(15) - 2) | 0b1));	//set LR to point to instruction after this one
		setReg(15, LR);				//set PC to old LR contents (plus the offset)
	}
}