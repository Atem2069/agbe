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
	std::string romName = "rom\\m3_demo.gba";
	Logger::getInstance()->msg(LoggerSeverity::Info, "ROM Path: " + romName);

	std::vector<uint8_t> romData = readFile(romName.c_str());
	std::vector<uint8_t> biosData = readFile("rom\\gba_bios.bin");

	m_interruptManager = std::make_shared<InterruptManager>();
	m_ppu = std::make_shared<PPU>(m_interruptManager);
	m_input = std::make_shared<Input>();
	m_bus = std::make_shared<Bus>(biosData, romData, m_interruptManager, m_ppu,m_input);
	m_cpu = std::make_shared<ARM7TDMI>(m_bus,m_interruptManager);

	Logger::getInstance()->msg(LoggerSeverity::Info, "Inited GBA instance!");
}

std::vector<uint8_t> GBA::readFile(const char* name)
{
	// open the file:
	std::ifstream file(name, std::ios::binary);

	// Stop eating new lines in binary mode!!!
	file.unsetf(std::ios::skipws);

	// get its size:
	std::streampos fileSize;

	file.seekg(0, std::ios::end);
	fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	// reserve capacity
	std::vector<uint8_t> vec;
	vec.reserve(fileSize);

	// read the data:
	vec.insert(vec.begin(),
		std::istream_iterator<uint8_t>(file),
		std::istream_iterator<uint8_t>());

	return vec;
}