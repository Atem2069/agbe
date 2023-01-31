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

	const int registerSizeLUT[8] = { 0,8,56,24,0,0,0,0 };	//amount of bytes to expect to be read/written for each RTC register
	const std::string registerNameLUT[8] = { "Reset","Control","Date/Time","Time","Unknown","Unknown","IRQ","Unknown" };

	uint8_t controlReg = 0b01000000;	//does rtc startup in 12hr or 24hr mode ?
	uint32_t dateReg = 0x01010122;
	uint32_t timeReg = 0x00010005;
};