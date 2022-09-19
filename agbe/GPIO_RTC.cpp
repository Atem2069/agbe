#include"GPIO_RTC.h"

RTC::RTC()
{

}

RTC::~RTC()
{

}

uint8_t RTC::read(uint32_t address)
{

	//todo
	return 0x0;
}

//according to gbatek: rom space writes can only be 16/32 bit..hmm
void RTC::write16(uint32_t address, uint16_t value)
{

}

void RTC::write32(uint32_t address, uint32_t value)
{
	//probs just handle by splitting into two 16 bit writes if necessary (e.g. if C4 then C4,C6; if C6 then C6,C8)
	Logger::getInstance()->msg(LoggerSeverity::Warn, "32 bit write to rtc register...");
}