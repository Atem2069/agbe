#include"ARM7TDMI.h"

void ARM7TDMI::setLogicalFlags(uint32_t result, int carry)
{
	if (carry != -1)
		m_setCarryFlag(carry);
	m_setZeroFlag(result == 0);
	m_setNegativeFlag(((result >> 31) & 0b1));
}

void ARM7TDMI::setArithmeticFlags(uint32_t input, uint64_t operand, uint32_t result, bool addition, bool carryIn)
{
	//truthfully: copypasted (bc arithmetic flags are magic to me for the most part)
	//note: operand might need to be uint64_t?

	uint64_t operandBeforeCarry = operand;

	uint64_t carryLut[2] = { m_getCarryFlag()+1,m_getCarryFlag()};
	uint64_t carryMask = (carryIn << 1) | carryIn;
	operand += ((uint64_t)carryLut[addition] & carryMask);

	if (operand == 0x100000001)
	{
		addition = true;
		operand = 1;
	}

	m_setNegativeFlag(((result >> 31) & 0b1));
	m_setZeroFlag(result == 0);

	//Carry flag - Addition
	if (addition)
		m_setCarryFlag((operand > (0xFFFFFFFF - input)));

	//Carry flag - Subtraction
	else if (!addition)
		m_setCarryFlag(!(operand > input));

	//Overflow flag
	uint8_t input_msb = (input & 0x80000000) ? 1 : 0;
	uint8_t operand_msb = (operandBeforeCarry & 0x80000000) ? 1 : 0;
	uint8_t result_msb = (result & 0x80000000) ? 1 : 0;

	if (addition)
	{
		if (input_msb != operand_msb) { m_setOverflowFlag(false); }

		else
		{
			if ((result_msb == input_msb) && (result_msb == operand_msb)) { m_setOverflowFlag(false); }
			else { m_setOverflowFlag(true); }
		}
	}

	else
	{
		if (input_msb == operand_msb) { m_setOverflowFlag(false); }

		else
		{
			if (result_msb == operand_msb) { m_setOverflowFlag(true); }
			else { m_setOverflowFlag(false); }
		}
	}
}