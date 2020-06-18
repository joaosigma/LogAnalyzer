#ifndef LA_FLAVORS_REPO_HPP
#define LA_FLAVORS_REPO_HPP

#include "log_line.hpp"

#include <string>
#include <vector>
#include <functional>
#include <string_view>

namespace la
{
	class FlavorsRepo
	{
	public:
		enum class Type : uint8_t
		{
			//known types
			Unknown,
			WCSCOMLib,
			WCSServer,
			WCSAndroidLogcat

			//any new types should be placed after this line
		};

		struct Info
		{
			std::function<bool(LogLine&)> parser;

			struct
			{
				std::string_view filter; //what types of files are valid
				std::string_view filterSort; //what part of the file should be used to order said file
				bool reverseSort{ false }; //reverse the order of files
				std::string_view lineIdentification; //regex to identify if the first line of a file corresponds to this type
			} filesFilter;
		};

	public:
		static bool translateLogLevel(char firstChar, LogLevel& logLevel);
		static int64_t translateTimestamp(const char* str);

	public:
		static std::vector<std::string> listFolderFiles(Type type, std::string_view folderPath);

		static size_t iterateFolderFiles(Type type, std::string_view folderPath, const std::function<void(std::string filePath)>& cb);

		static Type retrieveFileType(std::string_view filePath);

		static bool processLineData(Type type, std::string_view line, LogLine& out);
		static size_t processFileData(Type type, const void* data, size_t dataSize, std::vector<LogLine>& out);
	};
}

#endif
