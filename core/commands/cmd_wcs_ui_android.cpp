#include "cmd_wcs_ui_android.hpp"

#include "../lines_tools.hpp"

#include <nlohmann/json.hpp>

namespace la
{
	namespace
	{
		void cmdSummary(CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools)
		{
			auto& lines = linesTools.lines();
			if (lines.empty())
				return;

			auto& jResult = resultCtx.json();
			
			//TODO
		}

		void cmdBarks(CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools)
		{
			auto& lines = linesTools.lines();
			if (lines.empty())
				return;

			LinesTools::FilterCollection filter{
					LinesTools::FilterParam<LinesTools::FilterType::Tag, std::string_view, LogLine::MatchType::StartsWith>("UiAndroid.WMCWatchDog"),
					LinesTools::FilterParam<LinesTools::FilterType::Method, std::string_view>("bark") };

			std::vector<size_t> lineIndices;
			linesTools.windowIterate({ 0, lines.size() }, filter, [&lineIndices](size_t, LogLine, size_t lineIndex)
			{
				lineIndices.push_back(lineIndex);
				return true;
			});

			resultCtx.addLineIndices(lineIndices);
		}
	}

	CommandsRepo::Registry CommandsUIAndroid::genCommandsRegistry()
	{
		CommandsRepo::Registry registry;

		registry.tag = "UI Android";
		registry.registerCommandsCb = [](CommandsRepo::IRegisterCtx& registerCtx)
		{
			auto flavor = registerCtx.flavor();
			if ((flavor != FlavorsRepo::Type::WCSCOMLib) && (flavor != FlavorsRepo::Type::WCSAndroidLogcat))
				return;

			registerCtx.registerCommand({ "Summary", "Produce a quick summary of the entire logs", {},
				[](CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view) { return cmdSummary(resultCtx, linesTools); } });

			registerCtx.registerCommand({ "Bark!", "Find all barks", {},
				[](CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view) { return cmdBarks(resultCtx, linesTools); } });
		};

		return registry;
	}
}
