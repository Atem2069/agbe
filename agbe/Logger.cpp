#include"Logger.h"

Logger::Logger()
{
	//todo
}

Logger* Logger::getInstance()
{
	if (!instance)
		instance = new Logger();


	return instance;
}

void Logger::msg(LoggerSeverity severity, std::string msg, const std::source_location location)
{
	std::string funcOrigin = location.function_name();
	std::string prefix = "[" + funcOrigin + "]";
	switch (severity)
	{
	case LoggerSeverity::Error: prefix += "[ERROR]"; break;
	case LoggerSeverity::Warn: prefix += "[WARN]"; break;
	case LoggerSeverity::Info:prefix += "[INFO]"; break;
	}

	prefix += msg;

	m_msgLog.push(prefix);
	if (m_msgLog.size() > 1000)
		m_msgLog.pop();
	

	//if (severity == LoggerSeverity::Warn || severity == LoggerSeverity::Error)
		std::cout << prefix << '\n';

}

void Logger::dumpToConsole()
{
	while (!m_msgLog.empty())
	{
		std::cout << m_msgLog.front() << std::endl;
		m_msgLog.pop();
	}
}

void Logger::dumpToFile(std::string fileName)
{
	std::ofstream writeHandle(fileName);
	while (!m_msgLog.empty())
	{
		writeHandle << m_msgLog.front() << std::endl;
		m_msgLog.pop();
	}

	writeHandle.close();
}

Logger* Logger::instance = nullptr;	//initialize static instance to null explicitly inside a translation unit