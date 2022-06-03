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
	uint8_t destRegIndex = m_currentOpcode & 0b111;
	uint8_t srcRegIndex = ((m_currentOpcode >> 3) & 0b111);
	uint8_t op = ((m_currentOpcode >> 9) & 0b1);
	bool immediate = ((m_currentOpcode >> 10) & 0b1);

	uint32_t operand1 = getReg(srcRegIndex);
	uint32_t operand2 = 0;
	uint32_t result = 0;

	if (immediate)
		operand2 = ((m_currentOpcode >> 6) & 0b111);
	else
	{
		uint8_t tmp = ((m_currentOpcode >> 6) & 0b111);
		operand2 = getReg(tmp);
	}

	switch (op)
	{
	case 0:
		result = operand1 + operand2;
		setReg(destRegIndex, result);
		setArithmeticFlags(operand1, operand2, result, true);
		break;
	case 1:
		result = operand1 - operand2;
		setReg(destRegIndex, result);
		setArithmeticFlags(operand1, operand2, result, false);
		break;
	}

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
	uint8_t srcDestRegIdx = m_currentOpcode & 0b111;
	uint8_t op2RegIdx = ((m_currentOpcode >> 3) & 0b111);
	uint8_t operation = ((m_currentOpcode >> 6) & 0xF);

	uint32_t operand1 = getReg(srcDestRegIdx);
	uint32_t operand2 = getReg(op2RegIdx);
	uint32_t result = 0;

	int tempCarry = -1;
	uint32_t carryIn = m_getCarryFlag() & 0b1;

	switch (operation)
	{
	case 0:	//AND
		result = operand1 & operand2;
		setReg(srcDestRegIdx, result);
		setLogicalFlags(result, -1);
		break;
	case 1:	//EOR
		result = operand1 ^ operand2;
		setReg(srcDestRegIdx, result);
		setLogicalFlags(result, -1);
		break;
	case 2:	//LSL
		result = LSL(operand1, operand2, tempCarry);
		setReg(srcDestRegIdx, result);
		setLogicalFlags(result, tempCarry);
		break;
	case 3:	//LSR
		result = LSR(operand1, operand2, tempCarry);
		setReg(srcDestRegIdx, result);
		setLogicalFlags(result, tempCarry);
		break;
	case 4:	//ASR
		result = ASR(operand1, operand2, tempCarry);
		setReg(srcDestRegIdx, result);
		setLogicalFlags(result, tempCarry);
		break;
	case 5:	//ADC
		result = operand1 + operand2 + carryIn;
		setReg(srcDestRegIdx, result);
		setArithmeticFlags(operand1, operand2, result, true);
		break;
	case 6:	//SBC
		result = operand1 - operand2 - (!carryIn);
		setReg(srcDestRegIdx, result);
		setArithmeticFlags(operand1, operand2 + (!carryIn), result, false);
		break;
	case 7:	//ROR
		result = ROR(operand1, operand2, tempCarry);
		setReg(srcDestRegIdx, result);
		setLogicalFlags(result, tempCarry);
		break;
	case 8:	//TST
		result = operand1 & operand2;
		setLogicalFlags(result, -1);
		break;
	case 9:	//NEG
		result = (~operand2) + 1;
		setReg(srcDestRegIdx, result);
		setArithmeticFlags(0, operand2, result, false);	//not sure about this
		break;
	case 10: //CMP
		result = operand1 - operand2;
		setArithmeticFlags(operand1, operand2, result, false);
		break;
	case 11: //CMN
		result = operand1 + operand2;
		setArithmeticFlags(operand1, operand2, result, true);
		break;
	case 12: //ORR
		result = operand1 | operand2;
		setReg(srcDestRegIdx, result);
		setLogicalFlags(result, -1);
		break;
	case 13: //MUL
		result = operand1 * operand2;
		setReg(srcDestRegIdx, result);
		setLogicalFlags(result, -1);	//hmm...
		break;
	case 14: //BIC
		result = operand1 & (~operand2);
		setReg(srcDestRegIdx, result);
		setLogicalFlags(result, -1);
		break;
	case 15: //MVN
		result = (~operand2);
		setReg(srcDestRegIdx, result);
		setLogicalFlags(result, -1);
		break;
	}
}

void ARM7TDMI::Thumb_HiRegisterOperations()
{
	uint8_t srcRegIdx = ((m_currentOpcode >> 3) & 0b111);
	uint8_t dstRegIdx = m_currentOpcode & 0b111;
	uint8_t operation = ((m_currentOpcode >> 8) & 0b11);

	if ((m_currentOpcode >> 7) & 0b1)	//set them as 'high' registers
		dstRegIdx += 8;
	if ((m_currentOpcode >> 6) & 0b1)
		srcRegIdx += 8;

	uint32_t operand1 = getReg(dstRegIdx);	//check this? arm docs kinda confusing to read
	uint32_t operand2 = getReg(srcRegIdx);
	uint32_t result = 0;

	switch (operation)
	{
	case 0:
		result = operand1 + operand2;
		setReg(dstRegIdx, result);
		break;
	case 1:
		result = operand1 - operand2;
		setArithmeticFlags(operand1, operand2, result, false);
		break;
	case 2:
		result = operand2;
		setReg(dstRegIdx, result);
		break;
	case 3:
		if (!(operand2 & 0b1))
		{
			//enter arm
			CPSR &= 0xFFFFFFDF;	//unset T bit
			operand2 &= ~0b11;
			setReg(15, operand2);
		}
		else
		{
			//stay in thumb
			operand2 &= ~0b1;
			setReg(15, operand2);
		}
		break;
	}

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
	bool loadStore = ((m_currentOpcode >> 11) & 0b1);
	uint8_t baseRegIdx = ((m_currentOpcode >> 8) & 0b111);
	uint8_t regList = m_currentOpcode & 0xFF;
	uint8_t regListOriginal = regList;

	uint32_t base = getReg(baseRegIdx);

	if (loadStore)	//LDMIA
	{
		for (int i = 0; i < 8; i++)
		{
			if (regList & 0b1)
			{
				uint32_t cur = m_bus->read32(base);
				setReg(i, cur);
				base += 4;	//base always increments with this opcode
			}
			regList >>= 1;	//shift one to the right to check next register
		}
	}
	else			//STMIA
	{
		for (int i = 0; i < 8; i++)
		{
			if (regList & 0b1)
			{
				uint32_t cur = getReg(i);
				m_bus->write32(base, cur);
				base += 4;
			}
			regList >>= 1;
		}
	}

	if (regListOriginal == 0)
	{
		if (loadStore)
			setReg(15, m_bus->read32(base));
		else
			m_bus->write32(base, getReg(15));
		base += 0x40;
	}

	setReg(baseRegIdx, base);
	//TODO: writeback with rb in rlist has different behaviour that's unimplemented (see gbatek)

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