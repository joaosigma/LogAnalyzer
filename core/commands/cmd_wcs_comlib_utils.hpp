#ifndef LA_CMD_WCS_COMLIB_UTILS_HPP
#define LA_CMD_WCS_COMLIB_UTILS_HPP

#include "../lines_tools.hpp"

#include <vector>
#include <optional>

namespace la
{
	class CommandsCOMLibUtils final
	{
	public:
		struct TaskLineInfo {
			int64_t taskId;
			size_t firstLineIndex;
		};

		struct HTTPLineInfo {
			int64_t httpRequestId;
			size_t firstLineIndex;
		};

	public:
		static std::vector<LinesTools::LineIndexRange> executionsRanges(const LinesTools& linesTools);

		static std::vector<size_t> taskFullExecution(const LinesTools& linesTools, int64_t taskId, LinesTools::LineIndexRange lineRange);
		static std::optional<TaskLineInfo> taskAtLine(const LinesTools& linesTools, size_t lineIndex);

		static std::vector<size_t> httpRequestFullExecution(const LinesTools& linesTools, int64_t httpRequestId, LinesTools::LineIndexRange lineRange);
		static std::optional<HTTPLineInfo> httpRequestAtLine(const LinesTools& linesTools, size_t lineIndex);
	};
}

#endif
