#include"Logger.h"

class RTC
{
public:
	RTC();
	~RTC();

	uint8_t read(uint32_t address);

	//according to gbatek: rom space writes can only be 16/32 bit..hmm
	void write16(uint32_t address, uint16_t value);
	void write32(uint32_t address, uint32_t value);

private:
	uint8_t data = {};
	uint8_t directionMask = {};
	bool readWriteMask = false;
};