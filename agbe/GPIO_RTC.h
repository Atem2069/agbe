#include<ctime>
#include"Logger.h"

enum class GPIOState
{
	Ready,
	Command,
	Read,
	Write
};

class RTC
{
public:
	RTC();
	~RTC();

	uint8_t read(uint32_t address);

	//according to gbatek: rom space writes can only be 16/32 bit..hmm
	void write16(uint32_t address, uint16_t value);
	void write32(uint32_t address, uint32_t value);

	bool getRegistersReadable();

private:

	void m_writeDataRegister(uint8_t value);
	uint8_t m_reverseBits(uint8_t a);
	uint8_t m_convertToBCD(int val);
	void m_updateRTCRegisters();

	uint8_t data = {};
	uint8_t directionMask = {};
	bool readWriteMask = false;

	uint64_t m_dataLatch = 0;
	uint8_t m_command = 0;
	int m_shiftCount = 0;

	GPIOState m_state;

	const std::string registerNameLUT[8] = { "Reset","Control","Date/Time","Time","Unknown","Unknown","IRQ","Unknown" };

	uint8_t controlReg = 0x40;	//I think RTC starts in 24hr mode by default?
	uint32_t dateReg = 0;
	uint32_t timeReg = 0;
};