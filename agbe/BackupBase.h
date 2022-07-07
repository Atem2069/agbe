#pragma once

#include"Logger.h"
#include"Config.h"

enum class BackupType
{
	None,
	SRAM,
	EEPROM4K,
	EEPROM64K,
	FLASH512K,
	FLASH1M
};

class BackupBase
{
public:
	BackupBase() {};
	BackupBase(BackupType type) {};
	~BackupBase() {};

	virtual uint8_t read(uint32_t address) { return 0; };
	virtual void write(uint32_t address, uint8_t value) {};
protected:
	std::string m_saveName;
	int saveSize = 0;
	bool getSaveData(std::vector<uint8_t>& vec)
	{
		std::string saveName = Config::GBA.RomName;
		saveName.resize(saveName.size() - 3);	//assume end of filename is "gba", bit hacky :)
		saveName += "sav";

		m_saveName = saveName;

		// open the file:
		std::ifstream file(saveName, std::ios::binary);
		if (!file)
		{
			Logger::getInstance()->msg(LoggerSeverity::Info, "No savefile associated with the current game - generating new file!");
			return false;
		}

		// Stop eating new lines in binary mode!!!
		file.unsetf(std::ios::skipws);

		// get its size:
		std::streampos fileSize;

		file.seekg(0, std::ios::end);
		fileSize = file.tellg();
		file.seekg(0, std::ios::beg);

		// reserve capacity
		vec.reserve(fileSize);

		// read the data:
		vec.insert(vec.begin(),
			std::istream_iterator<uint8_t>(file),
			std::istream_iterator<uint8_t>());

		file.close();

		return true;
	}

	void writeSaveData(void* saveData, int sizeBytes)
	{
		Logger::getInstance()->msg(LoggerSeverity::Info, "Writing save data..");

		std::ofstream saveWriteHandle(m_saveName, std::ios::out | std::ios::binary);
		saveWriteHandle.write((const char*)saveData, sizeBytes);
		saveWriteHandle.close();

		Logger::getInstance()->msg(LoggerSeverity::Info, "Save data write completed!");
	}
};