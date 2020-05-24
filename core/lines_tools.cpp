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
		if (m_fileLineRanges.empty() || (targetRange.start >= targetRange.end))
			return {};

		bool startGroupFound{ false };
		for (const auto& curLineRange : m_fileLineRanges)
		{
			//if we are already outside the range end limit, no point in going
			if (curLineRange.start >= targetRange.end)
				break;

			const char* lineDataStart, * lineDataEnd;
			std::vector<LogLine>::const_iterator lineStart, lineEnd;

			//have we reached the right group?
			if (!startGroupFound && ((targetRange.start < curLineRange.start) || (targetRange.start >= curLineRange.end)))
				continue;

			//calculate some pointers
			if (!startGroupFound)
			{
				assert((targetRange.start >= curLineRange.start) && (targetRange.start < curLineRange.end));
				lineStart = m_lines.begin() + targetRange.start;
				lineEnd = m_lines.begin() + curLineRange.end - 1;

				lineDataStart = lineStart->data.start + startCharacterIndex;
				lineDataEnd = lineEnd->data.end;
				if (lineDataStart > lineDataEnd)
					lineDataStart = lineDataEnd;

				//we can reset this in case we need to search in the next group
				startGroupFound = true;
			}
			else
			{
				lineStart = m_lines.begin() + curLineRange.start;
				lineEnd = m_lines.begin() + curLineRange.end - 1;

				lineDataStart = lineStart->data.start;
				lineDataEnd = lineEnd->data.end;
			}

			//this can happend if the last search coincides with the end of the group
			if (lineDataStart == lineDataEnd)
				continue;

			//this group is valid to try and find the text
			auto targetPtr = cbSearch(lineDataStart, lineDataEnd);
			if (!targetPtr || (targetPtr == lineDataEnd))
				continue;

			//found it!

			std::vector<LogLine>::const_iterator itLine; //calculate in which line we found it
			{
				LogLine aux;
				aux.data.start = targetPtr;
				itLine = std::lower_bound(lineStart, lineEnd, aux, [](const auto& a, const auto& b) { return (a.data.start < b.data.start); });
			}

			//remember that lineEnd is actually valid, so don't check it

			if (itLine->data.start > targetPtr)
				itLine--;

			assert((targetPtr >= itLine->data.start) && (targetPtr < itLine->data.end)); //and where in the content the data starts

			//calculate indices
			SearchResult result{
				true,
				static_cast<size_t>(std::distance(m_lines.begin(), itLine)),
				static_cast<size_t>(std::distance(itLine->data.start, targetPtr))
			};

			if (result.lineIndex >= targetRange.end) //ignore result if we are outside range limit
				return {};

			return result;
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
