#include "flavors_repo.hpp"

#include "flavors/flavor_wcs_comlib.hpp"
#include "flavors/flavor_wcs_server.hpp"
#include "flavors/flavor_wcs_android_logcat.hpp"

#include <map>
#include <array>
#include <ctime>
#include <regex>
#include <tuple>
#include <chrono>
#include <fstream>
#include <charconv>
#include <filesystem>

namespace la
{
	namespace
	{
		std::array<std::tuple<FlavorsRepo::Type, FlavorsRepo::Info>, 3> Flavors{ {
				//known types
				{ FlavorsRepo::Type::WCSCOMLib, flavors::WCSCOMLib::genFlavorsRepoInfo() },
				{ FlavorsRepo::Type::WCSServer, flavors::WCSServer::genFlavorsRepoInfo() },
				{ FlavorsRepo::Type::WCSAndroidLogcat, flavors::WCSAndroidLogCat::genFlavorsRepoInfo() },

				//any new types should be placed after this line
			} };
	}

	bool FlavorsRepo::translateLogLevel(char firstChar, LogLevel& logLevel)
	{
		switch (firstChar)
		{
		case 't':
		case 'T':
			logLevel = LogLevel::Trace;
			return true;
		case 'd':
		case 'D':
			logLevel = LogLevel::Debug;
			return true;
		case 'i':
		case 'I':
			logLevel = LogLevel::Info;
			return true;
		case 'w':
		case 'W':
			logLevel = LogLevel::Warn;
			return true;
		case 'e':
		case 'E':
			logLevel = LogLevel::Error;
			return true;
		case 'f':
		case 'F':
			logLevel = LogLevel::Fatal;
			return true;

		default:
			break;
		}

		return false;
	}

	int64_t FlavorsRepo::translateTimestamp(const char* str)
	{
		std::tm timeData;
		int timeMilliseconds;

		if (auto [p, ec] = std::from_chars(str + 0, str + 4, timeData.tm_year); ec != std::errc())
			return 0;
		if (auto [p, ec] = std::from_chars(str + 5, str + 7, timeData.tm_mon); ec != std::errc())
			return 0;
		if (auto [p, ec] = std::from_chars(str + 8, str + 10, timeData.tm_mday); ec != std::errc())
			return 0;

		if (auto [p, ec] = std::from_chars(str + 11, str + 13, timeData.tm_hour); ec != std::errc())
			return 0;
		if (auto [p, ec] = std::from_chars(str + 14, str + 16, timeData.tm_min); ec != std::errc())
			return 0;
		if (auto [p, ec] = std::from_chars(str + 17, str + 19, timeData.tm_sec); ec != std::errc())
			return 0;

		if (auto [p, ec] = std::from_chars(str + 20, str + 23, timeMilliseconds); ec != std::errc())
			return 0;

		timeData.tm_year = timeData.tm_year - 1900;
		timeData.tm_mon--;

		auto timePoint = std::chrono::system_clock::from_time_t(std::mktime(&timeData));
		auto totalMillis = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count());

		return totalMillis + timeMilliseconds;
	}

	std::vector<std::string> FlavorsRepo::listFolderFiles(Type type, std::string_view folderPath)
	{
		std::vector<std::string> files;

		iterateFolderFiles(type, folderPath, [&files](std::string filePath)
		{
			files.push_back(std::move(filePath));
		});

		return files;
	}

	size_t FlavorsRepo::iterateFolderFiles(Type type, std::string_view folderPath, const std::function<void(std::string filePath)>& cb)
	{
		if (folderPath.empty() || (type == Type::Unknown) || !cb)
			return 0;

		auto path = std::filesystem::u8path(folderPath);
		if (!std::filesystem::is_directory(path))
			return 0;

		bool reverseSort{ false };
		std::regex regFilter, regSort;
		{
			auto it = std::find_if(Flavors.begin(), Flavors.end(), [type](const auto& knownFile) { return (std::get<0>(knownFile) == type); });
			if (it == Flavors.end())
				return 0;

			auto& flavorInfo = std::get<1>(*it);
			if (flavorInfo.filesFilter.filter.empty() || flavorInfo.filesFilter.filterSort.empty())
				return 0;

			regFilter = std::regex{ std::string{ flavorInfo.filesFilter.filter } };
			regSort = std::regex{ std::string{ flavorInfo.filesFilter.filterSort } };
			reverseSort = flavorInfo.filesFilter.reverseSort;
		}

		std::multimap<int32_t, std::filesystem::path> files;
		for (const auto& filePath : std::filesystem::directory_iterator(path))
		{
			if (!filePath.exists() || !filePath.is_regular_file())
				continue;

			auto fileName = filePath.path().filename().u8string();
			if (!std::regex_match(fileName, regFilter))
				continue;

			int32_t sortValue;
			{
				std::smatch matches;
				if (!std::regex_match(fileName, matches, regSort))
					continue;

				if (auto [p, ec] = std::from_chars(&*matches[1].first, &*matches[1].second, sortValue); ec != std::errc())
					continue;
			}

			files.emplace(sortValue, filePath.path());
		};

		if (!reverseSort)
		{
			for (const auto& [sort, path] : files)
				cb(path.u8string());
		}
		else
		{
			for (auto it = files.crbegin(); it != files.crend(); it++)
				cb(it->second.u8string());
		}

		return files.size();
	}

	FlavorsRepo::Type FlavorsRepo::retrieveFileType(std::string_view filePath)
	{
		auto path = std::filesystem::u8path(filePath);
		if (!std::filesystem::is_regular_file(path))
			return Type::Unknown;

		std::string fileLine;
		{
			std::ifstream in(path.native().c_str());
			std::getline(in, fileLine);
		}

		if (fileLine.empty())
			return Type::Unknown;

		for (const auto& [flavorType, flavorInfo] : Flavors)
		{
			if (flavorInfo.filesFilter.lineIdentification.empty())
				continue;

			std::smatch matches;
			if (std::regex_match(fileLine, matches, std::regex{ std::string{ flavorInfo.filesFilter.lineIdentification } }))
				return flavorType;
		}

		return Type::Unknown;
	}

	bool FlavorsRepo::processLineData(Type type, std::string_view line, LogLine& out)
	{
		if ((type == Type::Unknown) || line.empty())
			return false;

		std::memset(&out, 0, sizeof(LogLine));
		out.data.start = line.data();
		out.data.end = out.data.start + line.size();
		out.level = LogLevel::Fatal;

		for (const auto& [flavorType, flavorInfo] : Flavors)
		{
			if (flavorType != type)
				continue;

			return flavorInfo.parser(out);
		}

		return false;
	}

	size_t FlavorsRepo::processFileData(Type type, const void* data, size_t dataSize, std::vector<LogLine>& out)
	{
		if (!data || (dataSize <= 0) || (type == Type::Unknown))
			return 0;

		std::function<bool(LogLine&)> parser;
		for (const auto& [flavorType, flavorInfo] : Flavors)
		{
			if (flavorType != type)
				continue;

			parser = flavorInfo.parser;
			break;
		}
		if (!parser)
			return 0;

		auto walker = reinterpret_cast<const char*>(data);
		auto walkerEnd = walker + dataSize;

		size_t numLines{ 0 };
		while (walker < walkerEnd)
		{
			LogLine line;
			std::memset(&line, 0, sizeof(LogLine));
			line.data.start = walker;
			line.data.end = line.data.start;
			line.level = LogLevel::Fatal;

			//move to next line
			{
				while ((walker < walkerEnd) && (walker[0] != '\n') && (walker[0] != '\r'))
					walker++;

				line.data.end = walker;

				if ((walker < walkerEnd) && ((walker[0] == '\n') || (walker[0] == '\r')))
					walker++;
			}

			//consume '\0's for the next line
			while ((walker < walkerEnd) && (walker[0] == '\0'))
				walker++;

			//try to read a valid line
			auto success = parser(line);

			//we assume that an invalid line is actually content belonging to the previous line (multiline content)
			if (!success)
			{
				if (!out.empty())
				{
					auto& last = out.back();

					if ((last.sectionParams.size <= 0) && ((last.data.start + last.sectionMsg.offset + last.sectionMsg.size) == last.data.end))
						last.sectionMsg.size = static_cast<uint32_t>(line.data.end - last.data.start - last.sectionMsg.offset); //also append to the msg section
					last.data.end = line.data.end;
				}

				continue;
			}

			//done!
			out.push_back(line);
			numLines++;
		}

		return numLines;
	}
}
