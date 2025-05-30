#include"ARM7TDMI.h"


//start of Thumb instruction set
void ARM7TDMI::Thumb_MoveShiftedRegister()
{
	uint8_t operation = ((m_currentOpcode >> 11) & 0b11);
	uint8_t shiftAmount = ((m_currentOpcode >> 6) & 0b11111);
	uint8_t srcRegIdx = ((m_currentOpcode >> 3) & 0b111);
	uint8_t destRegIdx = m_currentOpcode & 0b111;

	uint32_t srcVal = R[srcRegIdx];
	uint32_t result = 0;
	int carry = -1;
	switch (operation)
	{
	case 0: result=LSL(srcVal, shiftAmount, carry); break;
	case 1: result=LSR(srcVal, shiftAmount, carry); break;
	case 2: result=ASR(srcVal, shiftAmount, carry); break;
	}

	setLogicalFlags(result, carry);
	R[destRegIdx] = result;
	m_scheduler->addCycles(1);	//not sure, but it is an 'alu op'
}

void ARM7TDMI::Thumb_AddSubtract()
{
	uint8_t destRegIndex = m_currentOpcode & 0b111;
	uint8_t srcRegIndex = ((m_currentOpcode >> 3) & 0b111);
	uint8_t op = ((m_currentOpcode >> 9) & 0b1);
	bool immediate = ((m_currentOpcode >> 10) & 0b1);

	uint32_t operand1 = R[srcRegIndex];
	uint32_t operand2 = 0;
	uint32_t result = 0;

	if (immediate)
		operand2 = ((m_currentOpcode >> 6) & 0b111);
	else
	{
		uint8_t tmp = ((m_currentOpcode >> 6) & 0b111);
		operand2 = R[tmp];
	}

	switch (op)
	{
	case 0:
		result = operand1 + operand2;
		R[destRegIndex] = result;
		setArithmeticFlags(operand1, operand2, result, true);
		break;
	case 1:
		result = operand1 - operand2;
		R[destRegIndex] = result;
		setArithmeticFlags(operand1, operand2, result, false);
		break;
	}
	m_scheduler->addCycles(1);
}

void ARM7TDMI::Thumb_MoveCompareAddSubtractImm()
{
	uint32_t offset = m_currentOpcode & 0xFF;
	uint8_t srcDestRegIdx = ((m_currentOpcode >> 8) & 0b111);
	uint8_t operation = ((m_currentOpcode >> 11) & 0b11);

	uint32_t operand1 = R[srcDestRegIdx];
	uint32_t result = 0;

	switch (operation)
	{
	case 0:
		result = offset;
		R[srcDestRegIdx] = result;
		setLogicalFlags(result, -1);
		break;
	case 1:
		result = operand1 - offset;
		setArithmeticFlags(operand1, offset, result, false);
		break;
	case 2:
		result = operand1 + offset;
		R[srcDestRegIdx] = result;
		setArithmeticFlags(operand1, offset, result, true);
		break;
	case 3:
		result = operand1 - offset;
		R[srcDestRegIdx] = result;
		setArithmeticFlags(operand1, offset, result, false);
		break;
	}
	m_scheduler->addCycles(1);	//not sure :P
}

void ARM7TDMI::Thumb_ALUOperations()
{
	uint8_t srcDestRegIdx = m_currentOpcode & 0b111;
	uint8_t op2RegIdx = ((m_currentOpcode >> 3) & 0b111);
	uint8_t operation = ((m_currentOpcode >> 6) & 0xF);

	uint32_t operand1 = R[srcDestRegIdx];
	uint32_t operand2 = R[op2RegIdx];
	uint32_t result = 0;

	int tempCarry = -1;
	uint32_t carryIn = m_getCarryFlag() & 0b1;

	switch (operation)
	{
	case 0:	//AND
		result = operand1 & operand2;
		R[srcDestRegIdx] = result;
		setLogicalFlags(result, -1);
		break;
	case 1:	//EOR
		result = operand1 ^ operand2;
		R[srcDestRegIdx] = result;
		setLogicalFlags(result, -1);
		break;
	case 2:	//LSL
		if (operand2)
			result = LSL(operand1, operand2, tempCarry);
		else
			result = operand1;
		R[srcDestRegIdx] = result;
		setLogicalFlags(result, tempCarry);
		break;
	case 3:	//LSR
		if (operand2)
			result = LSR(operand1, operand2, tempCarry);
		else
			result = operand1;
		R[srcDestRegIdx] = result;
		setLogicalFlags(result, tempCarry);
		break;
	case 4:	//ASR
		if (operand2)
			result = ASR(operand1, operand2, tempCarry);
		else
			result = operand1;
		R[srcDestRegIdx] = result;
		setLogicalFlags(result, tempCarry);
		break;
	case 5:	//ADC
		result = operand1 + operand2 + carryIn;
		R[srcDestRegIdx] = result;
		setArithmeticFlags(operand1, operand2, result, true);
		break;
	case 6:	//SBC
		result = operand1 - operand2 - (!carryIn);
		R[srcDestRegIdx] = result;
		setArithmeticFlags(operand1, (uint64_t)operand2 + (uint64_t)(!carryIn), result, false);	//<--this is sussy...
		break;
	case 7:	//ROR
		operand2 &= 0xFF;
		tempCarry = -1;
		if (operand2)
			result = ROR(operand1, operand2, tempCarry);
		else
			result = operand1;
		R[srcDestRegIdx] = result;
		setLogicalFlags(result, tempCarry);
		break;
	case 8:	//TST
		result = operand1 & operand2;
		setLogicalFlags(result, -1);
		break;
	case 9:	//NEG
		result = (~operand2) + 1;
		R[srcDestRegIdx] = result;
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
		R[srcDestRegIdx] = result;
		setLogicalFlags(result, -1);
		break;
	case 13: //MUL
		result = operand1 * operand2;
		R[srcDestRegIdx] = result;
		setLogicalFlags(result, -1);	//hmm...
		m_scheduler->addCycles(calculateMultiplyCycles(operand1, true));
		m_bus->tickPrefetcher(calculateMultiplyCycles(operand1, true));
		nextFetchNonsequential = true;	//mul has internal cycles, so next fetch is forced nonsequential for some reason.
		break;
	case 14: //BIC
		result = operand1 & (~operand2);
		R[srcDestRegIdx] = result;
		setLogicalFlags(result, -1);
		break;
	case 15: //MVN
		result = (~operand2);
		R[srcDestRegIdx] = result;
		setLogicalFlags(result, -1);
		break;
	}
	m_scheduler->addCycles(1);
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
		if (dstRegIdx == 15)
		{
			setReg(dstRegIdx, result & ~0b1);
			m_scheduler->addCycles(2);
		}
		break;
	case 1:
		result = operand1 - operand2;
		setArithmeticFlags(operand1, operand2, result, false);
		break;
	case 2:
		result = operand2;
		setReg(dstRegIdx, result);
		if (dstRegIdx == 15)
		{
			setReg(dstRegIdx, result & ~0b1);
			m_scheduler->addCycles(2);
		}
		break;
	case 3:
		if (!(operand2 & 0b1))
		{
			//enter arm
			CPSR &= 0xFFFFFFDF;	//unset T bit
			m_inThumbMode = false;
			operand2 &= ~0b11;
			setReg(15, operand2);
			m_scheduler->addCycles(2);
		}
		else
		{
			//stay in thumb
			operand2 &= ~0b1;
			setReg(15, operand2);
			m_scheduler->addCycles(2);
		}
		break;
	}
	m_scheduler->addCycles(1);
}

void ARM7TDMI::Thumb_PCRelativeLoad()
{
	uint32_t offset = (m_currentOpcode & 0xFF) << 2;
	uint32_t PC = R[15] & ~0b11;	//PC is force aligned to word boundary

	uint8_t destRegIdx = ((m_currentOpcode >> 8) & 0b111);

	uint32_t val = m_bus->read32(PC + offset, AccessType::Nonsequential);
	R[destRegIdx] = val;
	m_scheduler->addCycles(3);	//probs right? 
	m_bus->tickPrefetcher(1);
	nextFetchNonsequential = true;
}

void ARM7TDMI::Thumb_LoadStoreRegisterOffset()
{
	bool loadStore = ((m_currentOpcode >> 11) & 0b1);
	bool byteWord = ((m_currentOpcode >> 10) & 0b1);
	uint8_t offsetRegIdx = ((m_currentOpcode >> 6) & 0b111);
	uint8_t baseRegIdx = ((m_currentOpcode >> 3) & 0b111);
	uint8_t srcDestRegIdx = m_currentOpcode & 0b111;

	uint32_t base = R[baseRegIdx];
	base += R[offsetRegIdx];

	if (loadStore)	//load
	{
		if (byteWord)
		{
			uint32_t val = m_bus->read8(base, AccessType::Nonsequential);
			R[srcDestRegIdx] = val;
		}
		else
		{
			uint32_t val = m_bus->read32(base, AccessType::Nonsequential);
			if (base & 3)
				val = std::rotr(val, (base & 3) * 8);
			R[srcDestRegIdx] = val;
		}
		m_scheduler->addCycles(3);
		m_bus->tickPrefetcher(1);
	}
	else			//store
	{
		if (byteWord)
		{
			uint8_t val = R[srcDestRegIdx] & 0xFF;
			m_bus->write8(base, val, AccessType::Nonsequential);
		}
		else
		{
			uint32_t val = R[srcDestRegIdx];
			m_bus->write32(base, val, AccessType::Nonsequential);
		}
		m_scheduler->addCycles(2);
	}
	nextFetchNonsequential = true;
}

void ARM7TDMI::Thumb_LoadStoreSignExtended()
{
	uint8_t op = ((m_currentOpcode >> 10) & 0b11);
	uint8_t offsetRegIdx = ((m_currentOpcode >> 6) & 0b111);
	uint8_t baseRegIdx = ((m_currentOpcode >> 3) & 0b111);
	uint8_t srcDestRegIdx = m_currentOpcode & 0b111;

	uint32_t addr = R[baseRegIdx] + R[offsetRegIdx];


	if (op == 0)
	{
		uint16_t val = R[srcDestRegIdx] &0xFFFF;
		m_bus->write16(addr, val, AccessType::Nonsequential);
		m_scheduler->addCycles(2);
	}
	else if (op == 2)	//load halfword
	{
		uint32_t val = m_bus->read16(addr, AccessType::Nonsequential);
		if (addr & 0b1)
			val = std::rotr(val, 8);
		R[srcDestRegIdx] = val;
		m_scheduler->addCycles(3);
		m_bus->tickPrefetcher(1);
	}
	else if (op == 1)	//load sign extended byte
	{
		uint32_t val = m_bus->read8(addr, AccessType::Nonsequential);
		if (((val >> 7) & 0b1))
			val |= 0xFFFFFF00;
		R[srcDestRegIdx] = val;
		m_scheduler->addCycles(3);
		m_bus->tickPrefetcher(1);
	}
	else if (op == 3)   //load sign extended halfword
	{
		uint32_t val = 0;
		if (!(addr & 0b1))
		{
			val = m_bus->read16(addr, AccessType::Nonsequential);
			if (((val >> 15) & 0b1))
				val |= 0xFFFF0000;
		}
		else
		{
			val = m_bus->read8(addr, AccessType::Nonsequential);
			if (((val >> 7) & 0b1))
				val |= 0xFFFFFF00;
		}
		R[srcDestRegIdx] = val;
		m_scheduler->addCycles(3);
		m_bus->tickPrefetcher(1);
	}
	nextFetchNonsequential = true;
}

void ARM7TDMI::Thumb_LoadStoreImmediateOffset()
{
	bool byteWord = ((m_currentOpcode >> 12) & 0b1);
	bool loadStore = ((m_currentOpcode >> 11) & 0b1);
	uint32_t offset = ((m_currentOpcode >> 6) & 0b11111);
	uint8_t baseRegIdx = ((m_currentOpcode >> 3) & 0b111);
	uint8_t srcDestRegIdx = m_currentOpcode & 0b111;

	uint32_t baseAddr = R[baseRegIdx];
	if (!byteWord)		//if word, then it's a 7 bit address and word aligned so shl by 2
		offset <<= 2;
	baseAddr += offset;

	if (loadStore)	//Load value from memory
	{
		uint32_t val = 0;
		if (byteWord)
			val = m_bus->read8(baseAddr, AccessType::Nonsequential);
		else
		{
			val = m_bus->read32(baseAddr, AccessType::Nonsequential);
			if (baseAddr & 3)
				val = std::rotr(val, (baseAddr & 3) * 8);
		}
		R[srcDestRegIdx] = val;
		m_scheduler->addCycles(3);
		m_bus->tickPrefetcher(1);
	}
	else			//Store value to memory
	{
		uint32_t val = R[srcDestRegIdx];
		if (byteWord)
			m_bus->write8(baseAddr, val & 0xFF, AccessType::Nonsequential);
		else
			m_bus->write32(baseAddr, val, AccessType::Nonsequential);
		m_scheduler->addCycles(2);
	}

	nextFetchNonsequential = true;
}

void ARM7TDMI::Thumb_LoadStoreHalfword()
{
	bool loadStore = ((m_currentOpcode >> 11) & 0b1);
	uint32_t offset = ((m_currentOpcode >> 6) & 0x1F);
	uint8_t baseRegIdx = ((m_currentOpcode >> 3) & 0b111);
	uint8_t srcDestRegIdx = ((m_currentOpcode) & 0b111);

	offset <<= 1;	//6 bit address, might be sign extended? probs not

	uint32_t base = R[baseRegIdx];
	base += offset;

	if (loadStore)
	{
		uint32_t val = m_bus->read16(base, AccessType::Nonsequential);
		if (base & 0b1)
			val = std::rotr(val, 8);
		R[srcDestRegIdx] = val;
		m_scheduler->addCycles(3);
		m_bus->tickPrefetcher(1);
	}
	else
	{
		uint16_t val = R[srcDestRegIdx] &0xFFFF;
		m_bus->write16(base, val, AccessType::Nonsequential);
		m_scheduler->addCycles(2);
	}
	nextFetchNonsequential = true;
}

void ARM7TDMI::Thumb_SPRelativeLoadStore()
{
	bool loadStore = ((m_currentOpcode >> 11) & 0b1);
	uint8_t destRegIdx = ((m_currentOpcode >> 8) & 0b111);
	uint32_t offs = m_currentOpcode & 0xFF;
	offs <<= 2;

	uint32_t addr = getReg(13) + offs;

	if (loadStore)
	{
		uint32_t val = m_bus->read32(addr, AccessType::Nonsequential);
		if (addr & 3)
			val = std::rotr(val, (addr & 3) * 8);
		R[destRegIdx] = val;
		m_scheduler->addCycles(3);
		m_bus->tickPrefetcher(1);
	}
	else
	{
		uint32_t val = R[destRegIdx];
		m_bus->write32(addr, val, AccessType::Nonsequential);
		m_scheduler->addCycles(2);
	}
	nextFetchNonsequential = true;
}

void ARM7TDMI::Thumb_LoadAddress()
{
	uint32_t offset = m_currentOpcode & 0xFF;
	offset <<= 2;	//probs dont have to sign extend?

	uint8_t destRegIdx = ((m_currentOpcode >> 8) & 0b111);
	bool useSP = ((m_currentOpcode >> 11) & 0b1);

	if (useSP)	//SP used as base
	{
		uint32_t SP = getReg(13);
		SP += offset;
		R[destRegIdx] = SP;
	}
	else		//PC used as base
	{
		uint32_t PC = R[15] & ~0b11;
		PC += offset;
		R[destRegIdx] = PC;
	}
	m_scheduler->addCycles(1);	//probs right?
}

void ARM7TDMI::Thumb_AddOffsetToStackPointer()
{
	uint32_t offset = m_currentOpcode & 0b1111111;
	offset <<= 2;
	uint32_t SP = getReg(13);
	if (((m_currentOpcode >> 7) & 0b1))
		SP -= offset;
	else
		SP += offset;

	setReg(13, SP);
	m_scheduler->addCycles(1);	//probs right?
}

void ARM7TDMI::Thumb_PushPopRegisters()
{
	bool loadStore = ((m_currentOpcode >> 11) & 0b1);
	bool PCLR = ((m_currentOpcode >> 8) & 0b1);	//couldnt think of good abbreviation :/
	uint32_t regs = m_currentOpcode & 0xFF;

	uint32_t SP = getReg(13);

	bool firstTransfer = true;
	int transferCount = 0;

	if (loadStore) //Load - i.e. pop from stack
	{
		for (int i = 0; i < 8; i++)
		{
			if (((regs >> i) & 0b1))
			{
				transferCount++;
				uint32_t popVal = m_bus->read32(SP,(AccessType)!firstTransfer);
				R[i] = popVal;
				SP += 4;
				firstTransfer = false;
			}
		}

		if (PCLR)
		{
			uint32_t newPC = m_bus->read32(SP,(AccessType)!firstTransfer);
			setReg(15, newPC & ~0b1);
			SP += 4;
			m_scheduler->addCycles(2);
		}
		m_scheduler->addCycles(transferCount + 2);
		m_bus->tickPrefetcher(1);

	}
	else          //Store - i.e. push to stack
	{

		if (PCLR)
		{
			SP -= 4;
			m_bus->write32(SP, getReg(14), AccessType::Nonsequential);
			firstTransfer = false;
			m_scheduler->addCycles(1);
		}

		for (int i = 7; i >= 0; i--)
		{
			if (((regs >> i) & 0b1))
			{
				transferCount++;
				SP -= 4;
				m_bus->write32(SP, R[i], (AccessType)!firstTransfer);
				firstTransfer = false;
			}
		}

		m_scheduler->addCycles(transferCount + 1);
	}

	setReg(13, SP);
	nextFetchNonsequential = true;
}

void ARM7TDMI::Thumb_MultipleLoadStore()
{
	bool loadStore = (m_currentOpcode >> 11) & 0b1;
	uint8_t baseRegIdx = ((m_currentOpcode >> 8) & 0b111);
	uint8_t regList = m_currentOpcode & 0xFF;

	bool writeback = true;								//writeback implied, except for some odd LDM behaviour

	int transferCount = __popcnt16((uint16_t)regList);
	bool baseIsFirst = ((regList >> baseRegIdx) & 0b1) && !((regList << (8 - baseRegIdx)) & 0xFF);

	uint32_t base = R[baseRegIdx];
	uint32_t finalBase = base + (transferCount * 4);	//figure out final val of base address

	bool firstAccess = true;
	for (int i = 0; i < 8; i++)
	{
		if ((regList >> i) & 0b1)
		{
			if (loadStore)
			{
				uint32_t val = m_bus->read32(base, (AccessType)!firstAccess);
				R[i] = val;
				if (i == baseRegIdx)		//load with base included -> no writeback
					writeback = false;
			}

			else
			{
				uint32_t val = R[i];
				if (i == baseRegIdx && !baseIsFirst)
					val = finalBase;
				m_bus->write32(base, val, (AccessType)!firstAccess);
			}
			firstAccess = false;
			base += 4;
		}
	}

	if (transferCount)
	{
		int totalCycles = transferCount + ((loadStore) ? 2 : 1);
		m_scheduler->addCycles(totalCycles);
		if (loadStore)
			m_bus->tickPrefetcher(1);
	}
	else
	{
		if (loadStore)
		{
			//not sure about this timing.
			setReg(15, m_bus->read32(base, AccessType::Nonsequential));
			m_scheduler->addCycles(3);
			m_bus->tickPrefetcher(1);
		}
		else
		{
			m_bus->write32(base, R[15] + 2, AccessType::Nonsequential);	//+2 for pipeline effect
			m_scheduler->addCycles(2);
		}
		R[baseRegIdx] = base + 0x40;
		writeback = false;
	}

	if (writeback)
		R[baseRegIdx] = finalBase;
	nextFetchNonsequential = true;
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
	static constexpr auto conditionTable = genConditionCodeTable();
	bool conditionMet = (conditionTable[(CPSR >> 28) & 0xF] >> condition) & 0b1;
	if (!conditionMet)
	{
		m_scheduler->addCycles(1);
		return;
	}

	setReg(15, R[15] + offset);
	m_scheduler->addCycles(3);
}

void ARM7TDMI::Thumb_SoftwareInterrupt()
{
	//std::cout << "thumb swi" << (int)(m_currentOpcode&0xFF) << '\n';
	int swiId = m_currentOpcode & 0xFF;
	//svc mode bits are 10011
	uint32_t oldCPSR = CPSR;
	uint32_t oldPC = R[15] - 2;	//-2 because it points to next instruction

	CPSR &= 0xFFFFFFE0;	//clear mode bits (0-4)
	CPSR &= ~0b100000;	//clear T bit
	CPSR |= 0b10010011;	//set svc bits
	m_inThumbMode = false;
	swapBankedRegisters();

	setSPSR(oldCPSR);			//set SPSR_svc
	setReg(14, oldPC);			//Save old R15
	setReg(15, 0x00000008);		//SWI entry point is 0x08
	m_scheduler->addCycles(3);
}

void ARM7TDMI::Thumb_UnconditionalBranch()
{
	uint32_t offset = m_currentOpcode & 0x7FF;
	offset <<= 1;
	if (((offset >> 11) & 0b1))	//offset is two's complement so sign extend if necessary
		offset |= 0xFFFFF000;

	setReg(15, R[15] + offset);
	m_scheduler->addCycles(3);
}

void ARM7TDMI::Thumb_LongBranchWithLink()
{
	bool highLow = ((m_currentOpcode >> 11) & 0b1);
	uint32_t offset = m_currentOpcode & 0b11111111111;
	if (!highLow)	//H=0: leftshift offset by 12 and add to PC, then store in LR
	{
		offset <<= 12;
		if (offset & 0x400000) { offset |= 0xFF800000; }
		uint32_t res = R[15] + offset;
		setReg(14, res & ~0b1);
		m_scheduler->addCycles(1);
	}
	else			//H=1: leftshift by 1 and add to LR - then copy LR to PC. copy old PC (-2) to LR and set bit 0
	{
		offset <<= 1;
		uint32_t LR = getReg(14);
		LR += offset;
		setReg(14, ((R[15] - 2) | 0b1));	//set LR to point to instruction after this one
		setReg(15, LR);				//set PC to old LR contents (plus the offset)
		m_scheduler->addCycles(3);
	}
}