#include"ARM7TDMI.h"

uint32_t ARM7TDMI::LSL(uint32_t val, int shiftAmount, int& carry)
{
	if (shiftAmount == 0)
	{
		carry = m_getCarryFlag();
		return val;
	}
	int carryBit = 32 - shiftAmount;
	uint32_t res = val << shiftAmount;
	carry = ((val >> carryBit) & 0b1);
	return res;
}

uint32_t ARM7TDMI::LSR(uint32_t val, int shiftAmount, int& carry)
{
	if (shiftAmount == 0)
	{
		carry = ((val >> 31) & 0b1);
		return 0;
	}
	int carryBit = shiftAmount - 1;
	uint32_t res = val >> shiftAmount;
	carry = ((val >> carryBit) & 0b1);
	return res;
}

uint32_t ARM7TDMI::ASR(uint32_t val, int shiftAmount, int& carry)
{
	int32_t temp = (int32_t)val;	//use int32_t to ensure >> is arithmetic shift anyway
	if (shiftAmount == 0)
	{
		carry = ((val >> 31) & 0b1);
		return (temp >> 32);	//will be all 1's/0's according to the sign bit
	}
	int carryBit = shiftAmount - 1;
	temp >>= shiftAmount;
	carry = ((val >> carryBit) & 0b1);
	return temp;
}

uint32_t ARM7TDMI::ROR(uint32_t val, int shiftAmount, int& carry)
{
	if (shiftAmount == 0)
	{
		//RRX - rotates right by one and shifts in carry
		uint32_t msb = (m_getCarryFlag() & 0b1) << 31;
		uint32_t res = val >> 1;
		res |= msb;
		carry = (val & 0b1);
		return res;
	}
	//hmm
	uint32_t res = std::rotr(val, shiftAmount);
	int carryBit = shiftAmount - 1;
	carry = ((val >> carryBit) & 0b1);
	return res;
}