#include "lines_tools.hpp"

#include <cassert>
#include <algorithm>

namespace la
{
	const std::vector<LogLine>& LinesTools::lines() const
	{
		return m_lines;
	}

	LinesTools::SearchResult LinesTools::windowSearch(LineIndexRange targetRange, size_t startCharacterIndex, const std::function<const char* (const char*, const char*)>& cbSearch) const
	{
		if (targetRange.empty())
			return {};

		//as an optimization, do the first loop manually to avoid the check of "startCharacterIndex"
		if (startCharacterIndex > 0)
		{
			auto lineDataStart = m_lines[targetRange.start].data.start + startCharacterIndex;
			auto lineDataEnd = m_lines[targetRange.start].data.end;

			if (lineDataStart < lineDataEnd)
			{
				auto targetPtr = cbSearch(lineDataStart, lineDataEnd);
				if (targetPtr && (targetPtr < lineDataEnd))
				{
					SearchResult result{
						true,
						targetRange.start,
						static_cast<size_t>(targetPtr - m_lines[targetRange.start].data.start)
					};

					return result;
				}
			}

			targetRange.start++;
		}

		for (; targetRange.start < targetRange.end; targetRange.start++)
		{
			auto lineDataStart = m_lines[targetRange.start].data.start;
			auto lineDataEnd = m_lines[targetRange.start].data.end;

			auto targetPtr = cbSearch(lineDataStart, lineDataEnd);
			if (!targetPtr || (targetPtr == lineDataEnd))
				continue;

			return SearchResult{
				true,
				targetRange.start,
				static_cast<size_t>(targetPtr - m_lines[targetRange.start].data.start)
			};
		}

		return {};
	}

	std::vector<size_t> LinesTools::windowFindAll(LineIndexRange targetRange, std::string_view contentQuery) const
	{
		std::vector<size_t> lineIndices;

		if (contentQuery.empty())
			return lineIndices;

		std::boyer_moore_searcher bmSearcher(contentQuery.begin(), contentQuery.end());

		while (true)
		{
			auto result = windowSearch(targetRange, 0, [&bmSearcher](const char* dataStart, const char* dataEnd) { return std::search(dataStart, dataEnd, bmSearcher); });
			if (!result.valid)
				break;

			targetRange.start = result.lineIndex + 1;
			lineIndices.push_back(result.lineIndex);
		}

		return lineIndices;
	}

	std::optional<size_t> LinesTools::windowFindFirst(LineIndexRange targetRange, std::string_view contentQuery) const
	{
		if (contentQuery.empty())
			return std::nullopt;

		std::boyer_moore_searcher bmSearcher(contentQuery.begin(), contentQuery.end());

		auto result = windowSearch(targetRange, 0, [&bmSearcher](const char* dataStart, const char* dataEnd) { return std::search(dataStart, dataEnd, bmSearcher); });
		if (!result.valid)
			return std::nullopt;

		return result.lineIndex;
	}
}
