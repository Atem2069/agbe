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
		setReg(14, (oldR15 - 4) & ~0b11);	

	m_scheduler->addCycles(3);
}

void ARM7TDMI::ARM_DataProcessing()
{
	bool immediate = ((m_currentOpcode >> 25) & 0b1);
	uint8_t operation = ((m_currentOpcode >> 21) & 0xF);
	bool setConditionCodes = ((m_currentOpcode >> 20) & 0b1);
	uint8_t op1Idx = ((m_currentOpcode >> 16) & 0xF);
	uint8_t destRegIdx = ((m_currentOpcode >> 12) & 0xF);

	if (!setConditionCodes && (operation >> 2) == 0b10)
	{
		ARM_PSRTransfer();
		return;
	}

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
			m_scheduler->addCycles(1);
			nextFetchNonsequential = true;
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
	bool realign = true;
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
		if (setConditionCodes) { setArithmeticFlags(operand1, (uint64_t)operand2 + (uint64_t)carryIn, result, true); }
		break;
	case 6:	//SBC
		result = operand1 - operand2 + carryIn - 1;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setArithmeticFlags(operand1, (uint64_t)operand2 + (uint64_t)carryIn + 1, result, false); }
		break;
	case 7:	//RSC
		result = operand2 - operand1 + carryIn - 1;
		setReg(destRegIdx, result);
		if (setConditionCodes) { setArithmeticFlags(operand2, operand1 + carryIn + 1, result, false); }
		break;
	case 8:	//TST
		result = operand1 & operand2;
		realign = false;
		setLogicalFlags(result, shiftCarryOut);
		break;
	case 9:	//TEQ
		result = operand1 ^ operand2;
		realign = false;
		setLogicalFlags(result, shiftCarryOut);
		break;
	case 10: //CMP
		result = operand1 - operand2;
		realign = false;
		setArithmeticFlags(operand1, operand2, result, false);
		break;
	case 11: //CMN
		result = operand1 + operand2;
		realign = false;
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
		m_scheduler->addCycles(2);
		if (setCPSR)
		{
			uint32_t newPSR = getSPSR();
			CPSR = newPSR;

			if ((CPSR >> 5) & 0b1)
				setReg(15, getReg(15) & ~0b1);
		}

	}

	m_scheduler->addCycles(1);
}

void ARM7TDMI::ARM_PSRTransfer()
{
	bool PSR = ((m_currentOpcode >> 22) & 0b1);		//1: set spsr
	bool immediate = ((m_currentOpcode >> 25) & 0b1);
	bool opcode = ((m_currentOpcode >> 21) & 0b1);

	if (opcode) // MSR
	{
		uint32_t input = 0;
		uint32_t fieldMask = 0;
		if (m_currentOpcode & 0x80000) { fieldMask |= 0xFF000000; }		//mask bits depend on specific psr transfer op. fwiw mostly affects lower/upper byte
		if (m_currentOpcode & 0x40000) { fieldMask |= 0x00FF0000; }
		if (m_currentOpcode & 0x20000) { fieldMask |= 0x0000FF00; }
		if (m_currentOpcode & 0x10000) { fieldMask |= 0x000000FF; }

		if (immediate)
		{
			input = m_currentOpcode & 0xFF;
			uint8_t shift = (m_currentOpcode >> 8) & 0xF;
			int meaningless = 0;
			input = RORSpecial(input, shift, meaningless);
		}
		else
		{
			uint8_t srcReg = m_currentOpcode & 0xF;
			if (srcReg == 15)
			{

			}
			input = getReg(srcReg);
			input &= fieldMask;
		}

		if (PSR)
		{
			uint32_t temp = getSPSR();
			temp &= ~fieldMask;
			temp |= input;
			setSPSR(temp);
		}
		else
		{
			CPSR &= ~fieldMask;
			CPSR |= input;
			if (CPSR & 0x20)
			{
				// Switch to THUMB
				setReg(15, getReg(15) & ~0x1); // also flushes pipeline
			}
		}
	}
	else // MRS (transfer PSR to register)
	{
		uint8_t destReg = (m_currentOpcode >> 12) & 0xF;
		if (destReg == 15)
		{

		}
		if (PSR)
		{
			setReg(destReg, getSPSR());
		}
		else
		{
			setReg(destReg, CPSR);
		}
	}
	m_scheduler->addCycles(1);
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


	m_scheduler->addCycles(calculateMultiplyCycles(operand2, accumulate,true));
	nextFetchNonsequential = true;	//hm
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

	m_scheduler->addCycles(calculateMultiplyCycles(src2, accumulate, isSigned) + 1);
	nextFetchNonsequential = true;
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
		uint8_t swapVal = m_bus->read8(swapAddress,false);
		m_bus->write8(swapAddress, srcData & 0xFF,false);
		setReg(destRegIdx, swapVal);
		
	}

	else				//swap word
	{
		uint32_t swapVal = m_bus->read32(swapAddress,false);
		if (swapAddress & 3)
			swapVal = std::rotr(swapVal, (swapAddress & 3) * 8);
		m_bus->write32(swapAddress, srcData,false);
		setReg(destRegIdx, swapVal);
	}
	m_scheduler->addCycles(4);
	nextFetchNonsequential = true;
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
	m_scheduler->addCycles(3);
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
		if (srcDestRegIdx == 15)
			m_scheduler->addCycles(2);
		uint32_t val = 0;
		switch (operation)
		{
		case 0:
			Logger::getInstance()->msg(LoggerSeverity::Error, "SWP called from halfword transfer - opcode decoding is invalid!!!");
			break;
		case 1:
			val = m_bus->read16(base,false);
			if (base & 1)
				val = std::rotr(val, 8);
			setReg(srcDestRegIdx, val);
			break;
		case 2:
			val = m_bus->read8(base,false);
			if (((val >> 7) & 0b1))
				val |= 0xFFFFFF00;
			setReg(srcDestRegIdx, val);
			break;
		case 3:
			if (!(base & 0b1))
			{
				val = m_bus->read16(base,false);
				if (((val >> 15) & 0b1))
					val |= 0xFFFF0000;
			}
			else
			{
				val = m_bus->read8(base,false);
				if (((val >> 7) & 0b1))
					val |= 0xFFFFFF00;
			}
			setReg(srcDestRegIdx, val);
			break;
		}
		m_scheduler->addCycles(3);
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
			m_bus->write16(base, data & 0xFFFF,false);
			break;
		case 2:
			m_bus->write8(base, data & 0xFF,false);
			break;
		case 3:
			m_bus->write16(base, data & 0xFFFF,false);
			break;
		}
		m_scheduler->addCycles(2);
	}

	if (!prePost)
	{
		if (!upDown)
			base -= offset;
		else
			base += offset;
	}

	if (((!prePost) || (prePost && writeback)) && ((baseRegIdx != srcDestRegIdx) || (baseRegIdx == srcDestRegIdx && !loadStore)))
		setReg(baseRegIdx, base);
	
	nextFetchNonsequential = true;
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
		if (srcDestRegIdx == 15)
			m_scheduler->addCycles(2);
		uint32_t data = 0;
		switch (op)
		{
		case 0:
			Logger::getInstance()->msg(LoggerSeverity::Error, "Invalid halfword operation encoding");
			break;
		case 1:
			data = m_bus->read16(base,false);
			if (base & 1)
				data = std::rotr(data, 8);
			setReg(srcDestRegIdx, data);
			break;
		case 2:
			data = m_bus->read8(base,false);
			if (((data >> 7) & 0b1))	//sign extend byte if bit 7 set
				data |= 0xFFFFFF00;
			setReg(srcDestRegIdx, data);
			break;
		case 3:
			if (!(base & 0b1))
			{
				data = m_bus->read16(base,false);
				if (((data >> 15) & 0b1))
					data |= 0xFFFF0000;
			}
			else
			{
				data = m_bus->read8(base,false);
				if (((data >> 7) & 0b1))
					data |= 0xFFFFFF00;
			}
			setReg(srcDestRegIdx, data);
			break;
		}
		m_scheduler->addCycles(3);
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
			m_bus->write16(base, data & 0xFFFF,false);
			break;
		case 2:
			m_bus->write8(base, data & 0xFF,false);
			break;
		case 3:
			m_bus->write16(base, data & 0xFFFF,false);
			break;
		}
		m_scheduler->addCycles(2);
	}

	if (!prePost)
	{
		if (!upDown)
			base -= offset;
		else
			base += offset;
	}

	if (((!prePost) || (prePost && writeback)) && ((baseRegIdx != srcDestRegIdx) || (baseRegIdx == srcDestRegIdx && !loadStore)))
		setReg(baseRegIdx, base);

	nextFetchNonsequential = true;
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
		if (destRegIdx == 15)
			m_scheduler->addCycles(2);
		uint32_t val = 0;
		if (byteWord)
		{
			val = m_bus->read8(base,false);
		}
		else
		{
			val = m_bus->read32(base,false);
			if(base&3)
				val = std::rotr(val, (base & 3) * 8);
		}
		setReg(destRegIdx, val);
		m_scheduler->addCycles(3);
	}
	else //store value
	{
		uint32_t val = getReg(destRegIdx);
		if (destRegIdx == 15)	//R15 12 bytes ahead (instead of 8) if used for store
			val += 4;
		if (byteWord)
		{
			m_bus->write8(base, val & 0xFF,false);
		}
		else
		{
			m_bus->write32(base, val,false);
		}
		m_scheduler->addCycles(2);
	}

	if (!preIndex)
		base += offset;

	if (((!preIndex) || (preIndex && writeback)) && ((baseRegIdx != destRegIdx) || (baseRegIdx == destRegIdx && !loadStore)))
	{
		setReg(baseRegIdx, base);
	}

	nextFetchNonsequential = true;
}

void ARM7TDMI::ARM_Undefined()
{
	//Logger::getInstance()->msg(LoggerSeverity::Error, "Unimplemented");
	throw std::runtime_error("unimplemented");
}

void ARM7TDMI::ARM_BlockDataTransfer()
{
	bool prePost = ((m_currentOpcode >> 24) & 0b1);
	bool upDown = ((m_currentOpcode >> 23) & 0b1);
	bool psr = ((m_currentOpcode >> 22) & 0b1);
	bool writeBack = ((m_currentOpcode >> 21) & 0b1);
	bool loadStore = ((m_currentOpcode >> 20) & 0b1);
	uint8_t baseReg = ((m_currentOpcode >> 16) & 0xF);
	uint16_t r_list = m_currentOpcode & 0xFFFF;

	uint8_t oldMode = 0;
	// Force USR mode
	if (psr) { oldMode = CPSR & 0x1F; CPSR &= ~0xF; }

	uint32_t base_addr = getReg(baseReg);
	uint32_t old_base = base_addr;
	uint8_t transfer_reg = 0xFF;

	//Find out the first register in the Register List
	for (int x = 0; x < 16; x++)
	{
		if (r_list & (1 << x))
		{
			transfer_reg = x;
			x = 0xFF;
			break;
		}
	}

	bool baseIncluded = (r_list >> baseReg) & 0b1;
	int transferCount = 0;
	bool firstTransfer = true;	//used to make first access nonsequential

	//Load-Store with an ascending stack order, Up-Down = 1
	if ((upDown == 1) && (r_list != 0))
	{
		for (int x = 0; x < 16; x++)
		{
			if (r_list & (1 << x))
			{
				transferCount++;
				//Increment before transfer if pre-indexing
				if (prePost == 1) { base_addr += 4; }

				if (loadStore == 0)
				{
					//Store registers
					if ((x == transfer_reg) && (baseReg == transfer_reg)) { m_bus->write32(base_addr, old_base,!firstTransfer); }
					else { m_bus->write32(base_addr, getReg(x),!firstTransfer); }
				}
				else
				{
					//Load registers
					if ((x == transfer_reg) && (baseReg == transfer_reg)) { writeBack = 0; }
					setReg(x, m_bus->read32(base_addr,!firstTransfer));
				}
				firstTransfer = false;
				//Increment after transfer if post-indexing
				if (prePost == 0) { base_addr += 4; }
			}

			//Write back the into base register
			if (writeBack == 1 && (!loadStore || (loadStore && !baseIncluded))) { setReg(baseReg, base_addr); }
		}

		int cyclesToAdd = transferCount + ((loadStore) ? 2 : 1);
		m_scheduler->addCycles(cyclesToAdd);
	}

	//Load-Store with a descending stack order, Up-Down = 0
	else if ((upDown == 0) && (r_list != 0))
	{
		for (int x = 15; x >= 0; x--)
		{
			if (r_list & (1 << x))
			{
				transferCount++;
				//Decrement before transfer if pre-indexing
				if (prePost == 1) { base_addr -= 4; }

				//Store registers
				if (loadStore == 0)
				{
					if ((x == transfer_reg) && (baseReg == transfer_reg)) { m_bus->write32(base_addr, old_base,!firstTransfer); }
					else
					{
						uint32_t val = getReg(x);
						if (x == 15)
							val += 4;
						m_bus->write32(base_addr, val,!firstTransfer);
					}
				}

				//Load registers
				else
				{
					if ((x == transfer_reg) && (baseReg == transfer_reg)) { writeBack = 0; }
					setReg(x, m_bus->read32(base_addr,!firstTransfer));
				}

				firstTransfer = false;

				//Decrement after transfer if post-indexing
				if (prePost == 0) { base_addr -= 4; }
			}

			//Write back the into base register
			if (writeBack == 1 && (!loadStore || (loadStore && !baseIncluded))) { setReg(baseReg, base_addr); }
		}
		int cyclesToAdd = transferCount + ((loadStore) ? 2 : 1);
		m_scheduler->addCycles(cyclesToAdd);
	}
	else //Special case, empty RList
	{
		//Load R15
		if (loadStore == 0) { m_bus->write32(base_addr, getReg(15) + 4,false); }
		else //Store R15
		{
			setReg(15, m_bus->read32(base_addr,false));
		}

		//Add 0x40 to base address if ascending stack, writeback into base register
		if (upDown == 1) { setReg(baseReg, (base_addr + 0x40)); }

		//Subtract 0x40 from base address if descending stack, writeback into base register
		else { setReg(baseReg, (base_addr - 0x40)); }

	}

	// Restore old mode
	if (psr) { CPSR |= oldMode; }
	nextFetchNonsequential = true;
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
	//std::cout << "arm swi" << '\n';
	//svc mode bits are 10011
	uint32_t oldCPSR = CPSR;
	uint32_t oldPC = R[15] - 4;	//-4 because it points to next instruction

	CPSR &= 0xFFFFFFE0;	//clear mode bits (0-4)
	CPSR |= 0b0010011;	//set svc bits

	setSPSR(oldCPSR);			//set SPSR_svc
	setReg(14, oldPC);			//Save old R15
	setReg(15, 0x00000008);		//SWI entry point is 0x08
	m_scheduler->addCycles(3);
}