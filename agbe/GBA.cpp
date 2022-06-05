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
	while (!m_shouldStop)
	{
		m_cpu->step();
		m_input->update(*m_inp);
	}
}

void GBA::notifyDetach()
{
	m_shouldStop = true;	//stops the instance (bc otherwise this thread will go on forever when main thread ends)
}

void* GBA::getPPUData()
{
	return m_ppu->getDisplayBuffer();
}

void GBA::registerInput(std::shared_ptr<InputState> inp)
{
	m_inp = inp;
}

void GBA::m_initialise()
{
	Logger::getInstance()->msg(LoggerSeverity::Info, "Initializing new GBA instance");
	std::string romName = "rom\\armwrestler-gba-fixed.gba";
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

	m_interruptManager = std::make_shared<InterruptManager>();
	m_ppu = std::make_shared<PPU>(m_interruptManager);
	m_input = std::make_shared<Input>();
	m_bus = std::make_shared<Bus>(biosData, romData, m_interruptManager, m_ppu,m_input);
	m_cpu = std::make_shared<ARM7TDMI>(m_bus,m_interruptManager);

	Logger::getInstance()->msg(LoggerSeverity::Info, "Inited GBA instance!");
}