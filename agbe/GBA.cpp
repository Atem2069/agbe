#include"GBA.h"

GBA::GBA()
{
	m_scheduler = std::make_shared<Scheduler>();
	m_input = std::make_shared<Input>(m_scheduler);
	m_initialise();
}

GBA::~GBA()
{

}

void GBA::run()
{
	auto lastTime = std::chrono::high_resolution_clock::now();
	while (!m_shouldStop)
	{
		if (m_initialised)
		{
			m_cpu->step();

			if (m_ppu->getShouldSync())
			{
				auto curTime = std::chrono::high_resolution_clock::now();
				double timeDiff = std::chrono::duration<double, std::milli>(curTime - lastTime).count();
				Config::GBA.fps = 1.0 / (timeDiff/1000);
				if (!Config::GBA.disableVideoSync)
				{
					double target = ((280896.0) / (16777216.0)) * 1000;
					while (timeDiff < target)
					{
						curTime = std::chrono::high_resolution_clock::now();
						timeDiff = std::chrono::duration<double, std::milli>(curTime - lastTime).count();
					}
				}
				lastTime = curTime;
			}

		}
		if (Config::GBA.shouldReset)
		{
			vramCopyLock.lock();
			m_destroy();
			m_initialise();
			vramCopyLock.unlock();
		}
	}
}

void GBA::notifyDetach()
{
	m_shouldStop = true;	//stops the instance (bc otherwise this thread will go on forever when main thread ends)
}

void* GBA::getPPUData()
{
	vramCopyLock.lock();
	if(m_initialised)
		memcpy(safe_dispBuffer, m_ppu->getDisplayBuffer(), 240 * 160 * sizeof(uint32_t));
	vramCopyLock.unlock();
	return safe_dispBuffer;
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

	m_interruptManager = std::make_shared<InterruptManager>();
	m_ppu = std::make_shared<PPU>(m_interruptManager,m_scheduler);
	m_bus = std::make_shared<Bus>(biosData, romData, m_interruptManager, m_ppu,m_input,m_scheduler);
	m_cpu = std::make_shared<ARM7TDMI>(m_bus,m_interruptManager);

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

	return vec;
}