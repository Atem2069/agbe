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
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
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
	Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}