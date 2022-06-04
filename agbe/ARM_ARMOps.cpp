#include "ARM7TDMI.h"

void ARM7TDMI::ARM_Branch()
{
	bool link = ((m_currentOpcode >> 24) & 0b1);
	int32_t offset = m_currentOpcode & 0x00FFFFFF;
	offset <<= 2;	//now 26 bits
	if ((offset >> 25) & 0b1)	//if sign (bit 25) set, then must sign extend
		offset |= 0xFC000000;

	//set R15
	uint32_t oldR15 = getReg(15);
	setReg(15, getReg(15) + offset);
	if (link)
		setReg(14, oldR15 - 4);	

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
		int shiftAmount = ((m_currentOpcode >> 8) & 0xF);
		if (shiftAmount > 0)
			operand2 = RORSpecial(operand2, shiftAmount, shiftCarryOut);
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
			if (op1Idx == 15)
				operand1 += 4;
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
		//Logger::getInstance()->msg(LoggerSeverity::Info, "Dest reg was 15!!");
		if (setCPSR)
		{
			uint32_t newPSR = getSPSR();
			CPSR = newPSR;
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

	if (opBits == 0b1010011111)		//MSR
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
	else if (opBits == 0b1010001111)	//MSR
	{
		bool isImmediate = ((m_currentOpcode >> 25) & 0b1);
		bool modifySPSR = ((m_currentOpcode >> 22) & 0b1);
		uint32_t modifyVal = 0;
		if (isImmediate)
		{
			modifyVal = m_currentOpcode & 0xFF;
			int meaningless = 0;
			int shiftAmount = ((m_currentOpcode >> 8) & 0xF);
			modifyVal = RORSpecial(modifyVal, shiftAmount, meaningless);	//TODO: doublecheck if this should be used here (as it multiplies the shift amount by 2)
		}
		else
			modifyVal = getReg((m_currentOpcode & 0xF));

		modifyVal &= 0xF0000000;
		if (modifySPSR)
		{
			uint32_t curSPSR = getSPSR();
			curSPSR &= 0x0FFFFFFF;
			curSPSR |= modifyVal;
			setSPSR(curSPSR);
		}
		else
		{
			CPSR &= 0x0FFFFFFF;
			CPSR |= modifyVal;
		}
	}
	else	//MRS
	{
		bool fetchSPSR = ((m_currentOpcode >> 22) & 0b1);
		uint8_t destRegIdx = ((m_currentOpcode >> 12) & 0xF);
		if (fetchSPSR)
			setReg(destRegIdx, getSPSR());
		else
			setReg(destRegIdx, CPSR);
	}
}

void ARM7TDMI::ARM_Multiply()
{
	bool accumulate = ((m_currentOpcode >> 21) & 0b1);
	bool setConditions = ((m_currentOpcode >> 20) & 0b1);
	uint8_t destRegIdx = ((m_currentOpcode >> 16) & 0xF);
	uint8_t accumRegIdx = ((m_currentOpcode >> 12) & 0xF);
	uint8_t op2RegIdx = ((m_currentOpcode >> 8) & 0xF);
	uint8_t op1RegIdx = ((m_currentOpcode) & 0xF);

	uint32_t operand1 = getReg(op1RegIdx);
	uint32_t operand2 = getReg(op2RegIdx);
	uint32_t accum = getReg(accumRegIdx);

	uint32_t result = 0;

	if (accumulate)
		result = operand1 * operand2 + accum;
	else
		result = operand1 * operand2;

	setReg(destRegIdx, result);
	if (setConditions)
	{
		m_setNegativeFlag(((result >> 31) & 0b1));
		m_setZeroFlag(result == 0);
	}
}

void ARM7TDMI::ARM_MultiplyLong()
{
	bool isSigned = ((m_currentOpcode >> 22) & 0b1);
	bool accumulate = ((m_currentOpcode >> 21) & 0b1);
	bool setFlags = ((m_currentOpcode >> 20) & 0b1);
	uint8_t destRegHiIdx = ((m_currentOpcode >> 16) & 0xF);
	uint8_t destRegLoIdx = ((m_currentOpcode >> 12) & 0xF);
	uint8_t src2RegIdx = ((m_currentOpcode >> 8) & 0xF);
	uint8_t src1RegIdx = ((m_currentOpcode) & 0xF);

	uint64_t src1 = getReg(src1RegIdx);
	uint64_t src2 = getReg(src2RegIdx);

	uint64_t accumLow = getReg(destRegLoIdx);
	uint64_t accumHi = getReg(destRegHiIdx);
	uint64_t accum = ((accumHi << 32) | accumLow);

	if (isSigned)
	{
		if (((src1 >> 31) & 0b1))
			src1 |= 0xFFFFFFFF00000000;
		if (((src2 >> 31) & 0b1))
			src2 |= 0xFFFFFFFF00000000;
		//prob dont have to sign extend accum bc its inherent if its sign extended
	}

	uint64_t result = 0;
	if (accumulate)
		result = src1 * src2 + accum;
	else
		result = src1 * src2;

	if (setFlags)
	{
		m_setZeroFlag(result == 0);
		m_setNegativeFlag(((result >> 63) & 0b1));
	}

	setReg(destRegLoIdx, result & 0xFFFFFFFF);
	setReg(destRegHiIdx, ((result >> 32) & 0xFFFFFFFF));
}

void ARM7TDMI::ARM_SingleDataSwap()
{
	bool byteWord = ((m_currentOpcode >> 22) & 0b1);
	uint8_t baseRegIdx = ((m_currentOpcode >> 16) & 0xF);
	uint8_t destRegIdx = ((m_currentOpcode >> 12) & 0xF);
	uint8_t srcRegIdx = ((m_currentOpcode) & 0xF);

	uint32_t swapAddress = getReg(baseRegIdx);
	uint32_t srcData = getReg(srcRegIdx);

	if (byteWord)		//swap byte
	{
		uint8_t swapVal = m_bus->read8(swapAddress);
		m_bus->write8(swapAddress, srcData & 0xFF);
		setReg(destRegIdx, swapVal);
		
	}

	else				//swap word
	{
		uint32_t swapVal = m_bus->read32(swapAddress);
		if (swapAddress & 3)
			swapVal = std::rotr(swapVal, (swapAddress & 3) * 8);
		m_bus->write32(swapAddress, srcData);
		setReg(destRegIdx, swapVal);
	}
}


void ARM7TDMI::ARM_BranchExchange()
{
	uint8_t regIdx = m_currentOpcode & 0xF;
	uint32_t newAddr = getReg(regIdx);
	if (newAddr & 0b1)	//start executing THUMB instrs
	{
		CPSR |= 0b100000;	//set T bit in CPSR
		setReg(15, newAddr & ~0b1);
	}
	else				//keep going as ARM (but pipeline will be flushed)
	{
		setReg(15, newAddr & ~0b11);
	}
	//(setReg will cause a pipeline flush automatically if R15 written to, so no need here)
}

void ARM7TDMI::ARM_HalfwordTransferRegisterOffset()
{
	bool prePost = ((m_currentOpcode >> 24) & 0b1);
	bool upDown = ((m_currentOpcode >> 23) & 0b1);
	bool writeback = ((m_currentOpcode >> 21) & 0b1);
	bool loadStore = ((m_currentOpcode >> 20) & 0b1);
	uint8_t baseRegIdx = ((m_currentOpcode >> 16) & 0xF);
	uint8_t srcDestRegIdx = ((m_currentOpcode >> 12) & 0xF);
	uint8_t operation = ((m_currentOpcode >> 5) & 0b11);
	uint8_t offsetRegIdx = m_currentOpcode & 0xF;

	uint32_t base = getReg(baseRegIdx);
	uint32_t offset = getReg(offsetRegIdx);

	if (prePost)
	{
		if (upDown)
			base += offset;
		else
			base -= offset;
	}

	if (loadStore)
	{
		uint32_t val = 0;
		switch (operation)
		{
		case 0:
			Logger::getInstance()->msg(LoggerSeverity::Error, "SWP called from halfword transfer - opcode decoding is invalid!!!");
			break;
		case 1:
			val = m_bus->read16(base);
			setReg(srcDestRegIdx, val);
			break;
		case 2:
			val = m_bus->read8(base);
			if (((val >> 7) & 0b1))
				val |= 0xFFFFFF00;
			setReg(srcDestRegIdx, val);
			break;
		case 3:
			val = m_bus->read16(base);
			if (((val >> 15) & 0b1))
				val |= 0xFFFF0000;
			setReg(srcDestRegIdx, val);
			break;
		}
	}
	else						//store
	{
		uint32_t data = getReg(srcDestRegIdx);
		if ((operation == 1 || operation == 3) && srcDestRegIdx == 15)	//PC+12 when specified as src and halfword transfer taking place
			data += 4;
		switch (operation)
		{
		case 0:
			Logger::getInstance()->msg(LoggerSeverity::Error, "Invalid halfword operation encoding");
			break;
		case 1:
			m_bus->write16(base, data & 0xFFFF);
			break;
		case 2:
			m_bus->write8(base, data & 0xFF);
			break;
		case 3:
			m_bus->write16(base, data & 0xFFFF);
			break;
		}
	}

	if (!prePost)
	{
		if (!upDown)
			base -= offset;
		else
			base += offset;
	}

	if (writeback || !prePost)
		setReg(baseRegIdx, base);
}

void ARM7TDMI::ARM_HalfwordTransferImmediateOffset()
{
	bool prePost = ((m_currentOpcode >> 24) & 0b1);
	bool upDown = ((m_currentOpcode >> 23) & 0b1);
	bool writeback = ((m_currentOpcode >> 21) & 0b1);
	bool loadStore = ((m_currentOpcode >> 20) & 0b1);
	uint8_t baseRegIdx = ((m_currentOpcode >> 16) & 0xF);
	uint8_t srcDestRegIdx = ((m_currentOpcode >> 12) & 0xF);
	uint8_t offsHigh = ((m_currentOpcode >> 8) & 0xF);
	uint8_t op = ((m_currentOpcode >> 5) & 0b11);
	uint8_t offsLow = m_currentOpcode & 0xF;

	uint8_t offset = ((offsHigh << 4) | offsLow);

	uint32_t base = getReg(baseRegIdx);

	if (prePost)				//sort out pre-indexing
	{
		if (!upDown)
			base -= offset;
		else
			base += offset;
	}

	if (loadStore)				//load
	{
		uint32_t data = 0;
		switch (op)
		{
		case 0:
			Logger::getInstance()->msg(LoggerSeverity::Error, "Invalid halfword operation encoding");
			break;
		case 1:
			data = m_bus->read16(base);
			setReg(srcDestRegIdx, data);
			break;
		case 2:
			data = m_bus->read8(base);
			if (((data >> 7) & 0b1))	//sign extend byte if bit 7 set
				data |= 0xFFFFFF00;
			setReg(srcDestRegIdx, data);
			break;
		case 3:
			data = m_bus->read16(base);
			if (((data >> 15) & 0b1))
				data |= 0xFFFF0000;
			setReg(srcDestRegIdx, data);
			break;
		}
	}
	else						//store
	{
		uint32_t data = getReg(srcDestRegIdx);
		if ((op == 1 || op == 3) && srcDestRegIdx == 15)	//PC+12 when specified as src and halfword transfer taking place
			data += 4;
		switch (op)
		{
		case 0:
			Logger::getInstance()->msg(LoggerSeverity::Error, "Invalid halfword operation encoding");
			break;
		case 1:
			m_bus->write16(base, data & 0xFFFF);
			break;
		case 2:
			m_bus->write8(base, data & 0xFF);
			break;
		case 3:
			m_bus->write16(base, data & 0xFFFF);
			break;
		}
	}

	if (!prePost)
	{
		if (!upDown)
			base -= offset;
		else
			base += offset;
	}

	if (writeback || !prePost)
		setReg(baseRegIdx, base);
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
			if(base&3)
				val = std::rotr(val, (base & 3) * 8);
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
			m_bus->write8(base, val & 0xFF);
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
	bool prePost = ((m_currentOpcode >> 24) & 0b1);
	bool upDown = ((m_currentOpcode >> 23) & 0b1);
	bool forceUser = ((m_currentOpcode >> 22) & 0b1);
	bool transferPSR = false;
	bool writeBack = ((m_currentOpcode >> 21) & 0b1);
	bool loadStore = ((m_currentOpcode >> 20) & 0b1);
	uint8_t baseRegIdx = ((m_currentOpcode >> 16) & 0xF);
	uint16_t regList = m_currentOpcode & 0xFFFF;

	uint32_t base = getReg(baseRegIdx);
	uint32_t originalBase = base;	//save for some weird edge case we need to implement

	//figure out if base reg is first one 
	int firstReg = 0;
	for (int i = 0; i < 16; i++)
	{
		if (((originalBase >> i) & 0b1))
		{
			firstReg = i;
			i = 999;
			break;
		}
	}

	if (firstReg == baseRegIdx)
		Logger::getInstance()->msg(LoggerSeverity::Warn, "First reg in rlist is base reg!! unimplemented behaviour");

	if (upDown)
	{
		if (loadStore)	//LDMI(A)(B)
		{

			if (((regList >> 15) & 0b1) && forceUser)
			{
				forceUser = false;
				transferPSR = true;
			}

			for (int i = 0; i < 16; i++)
			{
				if (((regList >> i) & 0b1))
				{
					if (prePost)
						base += 4;
					uint32_t val = m_bus->read32(base);
					setReg(i, val,forceUser);
					if (!prePost)
						base += 4;

					//change CPSR if R15
					if (i == 15 && transferPSR)
					{
						uint32_t newpsr = getSPSR();
						CPSR = newpsr;
					}

				}
			}
		}

		else            //STMI(A)(B)
		{
			for (int i = 0; i < 16; i++)
			{
				if (((regList >> i) & 0b1))
				{
					if (prePost)
						base += 4;

					uint32_t val = getReg(i,forceUser);
					m_bus->write32(base, val);

					if (!prePost)
						base += 4;
				}
			}
		}
	}

	else		//FWIW this implementation is wrong. the correct behaviour is to write increasing addresses (classic nes series will break if we write descending..!)
	{
		if (loadStore)	//LDMD(A)(B)
		{
			if (((regList >> 15) & 0b1) && forceUser)
			{
				forceUser = false;
				transferPSR = true;
			}

			for (int i = 15; i >= 0; i--)
			{
				if (((regList >> i) & 0b1))
				{
					if (prePost)
						base -= 4;

					uint32_t val = m_bus->read32(base);
					setReg(i, val, forceUser);

					if (!prePost)
						base -= 4;
				}
			}

			if (((regList >> 15) & 0b1) && transferPSR)
			{
				uint32_t newpsr = getSPSR();
				CPSR = newpsr;
			}
		}

		else			//STMD(A)(B)
		{
			for (int i = 15; i >= 0; i--)
			{
				if (((regList >> i) & 0b1))
				{
					if (prePost)
						base -= 4;

					uint32_t val = getReg(i, forceUser);
					m_bus->write32(base, val);

					if (!prePost)
						base -= 4;
				}
			}
		}
	}

	if (writeBack)
		setReg(baseRegIdx, base);
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
	//svc mode bits are 10011
	uint32_t oldCPSR = CPSR;
	uint32_t oldPC = R[15] - 4;	//-4 because it points to next instruction

	CPSR &= 0xFFFFFFE0;	//clear mode bits (0-4)
	CPSR |= 0b10011;	//set svc bits

	setSPSR(oldCPSR);			//set SPSR_svc
	setReg(14, oldPC);			//Save old R15
	setReg(15, 0x00000008);		//SWI entry point is 0x08
}