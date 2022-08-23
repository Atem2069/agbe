#include"GBA.h"

GBA::GBA()
{
	m_scheduler = std::make_shared<Scheduler>();
	m_input = std::make_shared<Input>();
	m_scheduler->addEvent(Event::Frame, &GBA::onEvent, (void*)this, 280896);
	expectedNextFrame = 280896;
	m_initialise();
}

GBA::~GBA()
{

}

void GBA::run()
{
	auto lastTime = std::chrono::high_resolution_clock::now();
	m_lastTime = std::chrono::high_resolution_clock::now();
	while (!Config::GBA.shouldReset)
	{
		m_cpu->step();
	}
}

void GBA::frameEventHandler()
{
	auto curTime = std::chrono::high_resolution_clock::now();
	double timeDiff = std::chrono::duration<double, std::milli>(curTime - m_lastTime).count();
	Config::GBA.fps = 1.0 / (timeDiff / 1000);
	if (!Config::GBA.disableVideoSync)
	{
		static constexpr double target = ((280896.0) / (16777216.0)) * 1000;
		while (timeDiff < target)
		{
			curTime = std::chrono::high_resolution_clock::now();
			timeDiff = std::chrono::duration<double, std::milli>(curTime - m_lastTime).count();
		}
	}
	m_lastTime = curTime;
	m_ppu->updateDisplayOutput();	//maybe just move to vblank instead..
	m_scheduler->addEvent(Event::Frame, &GBA::onEvent, (void*)this, m_scheduler->getEventTime() + 280896);

	m_input->tick();

}

void GBA::onEvent(void* context)
{
	GBA* thisPtr = (GBA*)context;
	thisPtr->frameEventHandler();
}

void GBA::notifyDetach()
{
	m_shouldStop = true;	//stops the instance (bc otherwise this thread will go on forever when main thread ends)
}

void* GBA::getPPUData()
{
	return PPU::m_safeDisplayBuffer;
}

void GBA::registerInput(std::shared_ptr<InputState> inp)
{
	m_inp = inp;
	m_input->registerInput(m_inp);
}

void GBA::m_destroy()
{
	if (!m_initialised)
		return;

	m_ppu.reset();
	m_bus.reset();
	m_cpu.reset();
	m_scheduler->invalidateAll();
	m_scheduler->addEvent(Event::Frame, &GBA::onEvent, (void*)this, 280896);
	expectedNextFrame = 280896;
}

void GBA::m_initialise()
{
	Logger::getInstance()->msg(LoggerSeverity::Info, "Initializing new GBA instance");
	std::string romName = Config::GBA.RomName;
	if (romName == "")
		return;
	Logger::getInstance()->msg(LoggerSeverity::Info, "ROM Path: " + romName);

	std::vector<uint8_t> romData = readFile(romName.c_str());
	std::string biosPath = Config::GBA.exePath + (std::string)"\\rom\\gba_bios.bin";

	std::vector<uint8_t> biosData = readFile(biosPath.c_str());

	m_interruptManager = std::make_shared<InterruptManager>(m_scheduler);
	m_ppu = std::make_shared<PPU>(m_interruptManager,m_scheduler);
	m_bus = std::make_shared<Bus>(biosData, romData, m_interruptManager, m_ppu,m_input,m_scheduler);
	m_cpu = std::make_shared<ARM7TDMI>(m_bus,m_interruptManager,m_scheduler);
	m_input->registerInterrupts(m_interruptManager);
	Logger::getInstance()->msg(LoggerSeverity::Info, "Inited GBA instance!");
	m_initialised = true;
	Config::GBA.shouldReset = false;
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

	file.close();

	return vec;
}