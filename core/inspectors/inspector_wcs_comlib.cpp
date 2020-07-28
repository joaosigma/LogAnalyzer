#include "inspector_wcs_comlib.hpp"

#include "../lines_tools.hpp"
#include "../commands/cmd_wcs_comlib_utils.hpp"

#include <set>
#include <regex>

namespace la
{
	namespace
	{
		void inspectExecutions(InspectorsRepo::IResultCtx& inspectionCtx, const LinesTools& linesTools)
		{
			auto& lines = linesTools.lines();

			for (const auto& range : CommandsCOMLibUtils::executionsRanges(linesTools))
			{
				if (range.empty())
					continue;

				inspectionCtx.addExecution(lines[range.start].toStr(), range);
			}
		}

		void inspectPanics(InspectorsRepo::IResultCtx& inspectionCtx, const LinesTools& linesTools)
		{
			auto& lines = linesTools.lines();

			const LinesTools::FilterCollection filter{
						LinesTools::FilterParam<LinesTools::FilterType::LogLevel, LogLevel>(LogLevel::Error),
						LinesTools::FilterParam<LinesTools::FilterType::Tag, std::string_view>("COMLib.Debug"),
						LinesTools::FilterParam<LinesTools::FilterType::Method, std::string_view>("panic") };

			linesTools.windowIterate({ 0, lines.size() }, filter, [&inspectionCtx](size_t, LogLine line, size_t lineIndex)
			{
				inspectionCtx.addWarning("Panic / Exception", line.getSectionMsg(), lineIndex);
				return true;
			});
		}

		void inspectBuildInfo(InspectorsRepo::IResultCtx& inspectionCtx, const LinesTools& linesTools)
		{
			auto& lines = linesTools.lines();

			const LinesTools::FilterCollection filter{
						LinesTools::FilterParam<LinesTools::FilterType::LogLevel, LogLevel>(LogLevel::Info),
						LinesTools::FilterParam<LinesTools::FilterType::Tag, std::string_view>("COMLib"),
						LinesTools::FilterParam<LinesTools::FilterType::Msg, std::string_view, LogLine::MatchType::StartsWith>("****** ") };

			std::regex regMatch(R"(\*\*\*\*\*\* \w* \d\d \d\d\d\d \d\d:\d\d:\d\d \* .+ \* \w+)");

			std::set<std::string_view> buildInfos;
			linesTools.windowIterate({ 0, lines.size() }, filter, [&regMatch, &buildInfos](size_t, LogLine line, size_t)
			{
				//just the one at the start of the executions (ignore the ones printed after a log rotation)
				if (std::regex_match(line.data.start + line.sectionMsg.offset, line.data.start + line.sectionMsg.offset + line.sectionMsg.size, regMatch))
					buildInfos.insert(line.getSectionMsg());

				return true;
			});

			for (const auto& info : buildInfos)
				inspectionCtx.addInfo("Build info", info);
		}

		void inspectUAs(InspectorsRepo::IResultCtx& inspectionCtx, const LinesTools& linesTools)
		{
			auto& lines = linesTools.lines();

			LinesTools::FilterCollection filter{
						LinesTools::FilterParam<LinesTools::FilterType::Tag, std::string_view>("COMLib.PJSIP") };

			std::regex regex{ R"(User-Agent: (\S+\/\S+ \S+\/\S+ \S+\/\S+ \S+\/\S+))", std::regex::ECMAScript | std::regex::icase };

			std::set<std::string> userAgents;
			linesTools.windowIterate({ 0, lines.size() }, filter, [&regex, &userAgents](size_t, LogLine line, size_t)
			{
				std::cmatch matches;
				if (std::regex_search(line.data.start + line.sectionMsg.offset, line.data.end, matches, regex))
					userAgents.insert(matches[1].str());

				return true;
			});

			for (const auto& ua : userAgents)
				inspectionCtx.addInfo("User-Agent", ua);
		}
	}

	InspectorsRepo::Registry InspectorsCOMLib::genInspectorRegistry()
	{
		InspectorsRepo::Registry registry;

		registry.registerInspectorCb = [](InspectorsRepo::IRegisterCtx& registerCtx)
		{
			auto flavor = registerCtx.flavor();
			if ((flavor != FlavorsRepo::Type::WCSCOMLib) && (flavor != FlavorsRepo::Type::WCSAndroidLogcat))
				return;

			InspectorsRepo::InspectorInfo inspector;
			inspector.executionCb = [](InspectorsRepo::IResultCtx& inspectionCtx, const LinesTools& linesTools)
			{
				if (linesTools.lines().empty())
					return;

				inspectExecutions(inspectionCtx, linesTools);
				inspectPanics(inspectionCtx, linesTools);
				inspectBuildInfo(inspectionCtx, linesTools);
				inspectUAs(inspectionCtx, linesTools);
			};

			registerCtx.registerInspector(std::move(inspector));
		};

		return registry;
	}
}
