#include"GBA.h"

GBA::GBA()
{
	m_initialise();
}

GBA::~GBA()
{

}

void GBA::run()
{
	while (true)
	{
		m_cpu->step();
	}
}

void GBA::m_initialise()
{
	Logger::getInstance()->msg(LoggerSeverity::Info, "Initializing new GBA instance");
	std::string romName = "rom\\armwrestler.gba";
	Logger::getInstance()->msg(LoggerSeverity::Info, "ROM Path: " + romName);

	std::vector<uint8_t> romData;
	std::ifstream romReadHandle(romName, std::ios::in | std::ios::binary);
	if (!romReadHandle)
		return;

	romReadHandle >> std::noskipws;
	while (!romReadHandle.eof())
	{
		unsigned char curByte;
		romReadHandle.read((char*)&curByte, sizeof(uint8_t));
		romData.push_back((uint8_t)curByte);
	}
	romReadHandle.close();


	std::vector<uint8_t> biosData;
	std::ifstream biosReadHandle("rom\\gba_bios.bin", std::ios::in | std::ios::binary);
	if (!biosReadHandle)
		return;

	biosReadHandle >> std::noskipws;
	while (!biosReadHandle.eof())
	{
		unsigned char curByte;
		biosReadHandle.read((char*)&curByte, sizeof(uint8_t));
		biosData.push_back((uint8_t)curByte);
	}
	biosReadHandle.close();

	m_bus = std::make_shared<Bus>(biosData, romData);
	m_cpu = std::make_shared<ARM7TDMI>(m_bus);

	Logger::getInstance()->msg(LoggerSeverity::Info, "Inited GBA instance!");
}