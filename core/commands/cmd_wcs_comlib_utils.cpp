#include "cmd_wcs_comlib_utils.hpp"

#include <array>
#include <cassert>

#include <fmt/format.h>

namespace la
{
	std::vector<LinesTools::LineIndexRange> CommandsCOMLibUtils::executionsRanges(const LinesTools& linesTools)
	{
		auto execs = linesTools.windowFindAll({ 0, linesTools.lines().size() }, R"(|COMLib:  | ******************************* log start *******************************)");

		if (!execs.empty() && (execs.front() == 0)) //skip if logs start on an execution
			execs.erase(execs.begin());

		if (execs.empty()) //there's just one big execution
			return { LinesTools::LineIndexRange{0, linesTools.lines().size()} };

		std::vector<LinesTools::LineIndexRange> ranges;

		size_t lastIndex{ 0 };
		for (auto curIndex : execs)
		{
			assert(lastIndex <= curIndex);

			if (lastIndex < curIndex) //avoid storing empty executions
				ranges.push_back({ lastIndex, curIndex });

			lastIndex = curIndex;
		}

		return ranges;
	}

	std::vector<size_t> CommandsCOMLibUtils::taskFullExecution(const LinesTools& linesTools, int64_t taskId, LinesTools::LineIndexRange lineRange)
	{
		std::vector<size_t> lineIndices;

		auto& lines = linesTools.lines();

		//find where the task is scheduled
		size_t taskStartLineIndex;
		{
			auto result = linesTools.windowFindFirst({ lineRange.start, lineRange.end }, fmt::format("| task scheduled | id={}; name=", taskId));
			if (!result.has_value() || !lines[*result].checkSectionTag<LogLine::MatchType::Exact>("COMLib.Scheduler"))
				return {};

			lineIndices.push_back(*result);
			taskStartLineIndex = *result; //as an optimization, we don't need to start at the beginning of the logs
		}

		//find where the task is finished (which migth not exist if the app was killed)
		size_t taskEndLineIndex{ lineRange.end };
		{
			auto result = linesTools.windowFindFirst({ taskStartLineIndex, lineRange.end }, fmt::format("| task finished | id={}; name=", taskId));
			if (result.has_value() && lines[*result].checkSectionTag<LogLine::MatchType::Exact>("COMLib.Scheduler"))
			{
				lineIndices.push_back(*result);
				taskEndLineIndex = *result; //as an optimization, we don't need to finish at the end of the logs
			}
		}

		//gather all non execution or finishing events
		{
			static std::array<std::string_view, 10> Queries{ {
				"| task waiting (sync) | id={}; waiting for=",
				"| task waiting (time) | id={}; ms=",
				"| task waiting (task) | id={}; waiting for=",
				"| task moving on (sync) | id={}; waited for=",
				"| task moving on (task) | id={}; waited for=",
				"| task cancelled | id={};",
				"| scheduler canceled a task that didn't have support to be canceled | id={}; name=",
				"| canceling task because task is already running | id={}; name=",
				"| ignoring task remove because task is already running | id={}; name=",
				"| removed task | id={}; name="
			} };

			for (const auto& query : Queries)
			{
				for (auto curLineIndex : linesTools.windowFindAll({ taskStartLineIndex, taskEndLineIndex }, fmt::format(query, taskId)))
				{
					if (lines[curLineIndex].checkSectionTag<LogLine::MatchType::Exact>("COMLib.Scheduler"))
						lineIndices.push_back(curLineIndex);
				}
			}
		}

		//gather all executions and execution steps
		for (auto curLineIndex : linesTools.windowFindAll({ taskStartLineIndex, taskEndLineIndex }, fmt::format("| task executing | id={}; name=", taskId)))
		{
			if (!lines[curLineIndex].checkSectionTag<LogLine::MatchType::Exact>("COMLib.Scheduler"))
				continue;

			lineIndices.push_back(curLineIndex);

			LinesTools::FilterCollection filter{
				LinesTools::FilterParam<LinesTools::FilterType::ThreadId, int32_t>(lines[curLineIndex].threadId) };

			linesTools.windowIterate({ curLineIndex + 1, taskEndLineIndex }, filter, [&lineIndices](size_t, LogLine line, size_t lineIndex)
			{
				if (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.Scheduler") && !line.checkSectionMsg<LogLine::MatchType::Exact>("task scheduled"))
					return false;

				lineIndices.push_back(lineIndex);
				return true;
			});
		}

		//gather the finishing steps
		for (auto curLineIndex : linesTools.windowFindAll({ taskStartLineIndex, taskEndLineIndex }, fmt::format("| task finishing | id={}; name=", taskId)))
		{
			if (!lines[curLineIndex].checkSectionTag<LogLine::MatchType::Exact>("COMLib.Scheduler"))
				continue;

			lineIndices.push_back(curLineIndex);

			LinesTools::FilterCollection filter{
				LinesTools::FilterParam<LinesTools::FilterType::ThreadId, int32_t>(lines[curLineIndex].threadId) };

			linesTools.windowIterate({ curLineIndex + 1, taskEndLineIndex }, filter, [&lineIndices](size_t, LogLine line, size_t lineIndex)
			{
				if (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.Scheduler") && !line.checkSectionMsg<LogLine::MatchType::Exact>("task scheduled"))
					return false;

				lineIndices.push_back(lineIndex);
				return true;
			});
		}

		std::sort(lineIndices.begin(), lineIndices.end());

		return lineIndices;
	}

	std::optional<int64_t> CommandsCOMLibUtils::taskAtLine(const LinesTools& linesTools, size_t lineIndex)
	{
		auto& lines = linesTools.lines();

		if (lineIndex >= lines.size())
			return std::nullopt;

		LinesTools::FilterCollection filter{
				LinesTools::FilterParam<LinesTools::FilterType::ThreadId, int32_t>(lines[lineIndex].threadId),
				LinesTools::FilterParam<LinesTools::FilterType::Tag, std::string_view>("COMLib.Scheduler"),
				LinesTools::FilterParam<LinesTools::FilterType::Msg, std::string_view>("task executing") };

		std::optional<int64_t> taskId;
		linesTools.iterateBackwards(lineIndex, filter, [&taskId](size_t, LogLine line, size_t)
		{
			int64_t id;
			if (!line.paramExtractAs<int64_t>("id", id))
				return true;

			taskId = id;
			return false;
		});

		return taskId;
	}

	std::vector<size_t> CommandsCOMLibUtils::httpRequestFullExecution(const LinesTools& linesTools, int64_t httpRequestId, LinesTools::LineIndexRange lineRange)
	{
		std::vector<size_t> lineIndices;

		//find where the HTTP request is scheduled
		size_t taskStartLineIndex;
		{
			auto result = linesTools.windowFindFirst({ lineRange.start, lineRange.end }, fmt::format("|COMLib.HTTP: asioProcessDispatcher | request new | id={}; method=", httpRequestId));
			if (!result.has_value())
				return {};

			lineIndices.push_back(*result);
			taskStartLineIndex = *result; //as an optimization, we don't need to start at the beginning of the logs
		}

		//find where the HTTP request is finished (which migth not exist if the app was killed)
		size_t taskEndLineIndex{ lineRange.end };
		{
			auto result = linesTools.windowFindFirst({ taskStartLineIndex, lineRange.end }, fmt::format("|COMLib.HTTP: asioProcessTerminated | request finished | requestId={}; result=", httpRequestId));
			if (result.has_value())
			{
				lineIndices.push_back(*result);
				taskEndLineIndex = *result; //as an optimization, we don't need to finish at the end of the logs
			}
		}

		//gather all execution steps
		{
			LinesTools::FilterCollection filter{
					LinesTools::FilterParam<LinesTools::FilterType::Tag, std::string_view>("COMLib.HTTP"),
					LinesTools::FilterParam<LinesTools::FilterType::Method, std::string_view>("curlDebugCallback") };

			linesTools.windowIterate({ taskStartLineIndex, taskEndLineIndex }, filter, [&lineIndices, httpRequestId](size_t, LogLine line, size_t lineIndex)
			{
				if (line.paramCheck<int64_t>("request", httpRequestId))
					lineIndices.push_back(lineIndex);

				return true;
			});
		}

		std::sort(lineIndices.begin(), lineIndices.end());

		return lineIndices;
	}

	std::optional<int64_t> CommandsCOMLibUtils::httpRequestAtLine(const LinesTools& linesTools, size_t lineIndex)
	{
		auto& lines = linesTools.lines();

		if (lineIndex >= lines.size())
			return std::nullopt;

		auto& targetLine = lines[lineIndex];
		if (!targetLine.checkSectionTag<LogLine::MatchType::Exact>("COMLib.HTTP"))
			return std::nullopt;

		if (targetLine.checkSectionMethod<LogLine::MatchType::Exact>("curlDebugCallback"))
		{
			int64_t httpRequestId;
			if (targetLine.paramExtractAs<int64_t>("request", httpRequestId))
				return httpRequestId;
		}
		else if (targetLine.checkSectionMethod<LogLine::MatchType::Exact>("asioProcessDispatcher") || targetLine.checkSectionMethod<LogLine::MatchType::Exact>("asioProcessTerminated"))
		{
			int64_t httpRequestId;
			if (targetLine.paramExtractAs<int64_t>("requestId", httpRequestId))
				return httpRequestId;
		}

		return std::nullopt;
	}
}
