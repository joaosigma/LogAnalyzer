#include "cmd_wcs_server.hpp"

#include "../lines_tools.hpp"

#include <set>
#include <regex>
#include <optional>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace la
{
	namespace
	{
		void cmdMsg(CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view params)
		{
			auto& lines = linesTools.lines();
			if (lines.empty())
				return;

			std::vector<size_t> lineIndices;

			for (auto lineIndex : linesTools.windowFindAll({ 0, lines.size() }, params))
			{
				if (std::find(lineIndices.begin(), lineIndices.end(), lineIndex) != lineIndices.end())
					continue;

				auto& curLine = lines[lineIndex];

				LinesTools::FilterCollection filter{
					LinesTools::FilterParam<LinesTools::FilterType::ThreadName, std::string_view>(curLine.getSectionThreadName()) };

				linesTools.iterateBackwards(lineIndex - 1, filter, [&lineIndices](size_t, LogLine, size_t lineIndex)
				{
					lineIndices.push_back(lineIndex);
					return true;
				});

				linesTools.iterateForward(lineIndex, filter, [&lineIndices](size_t, LogLine, size_t lineIndex)
				{
					lineIndices.push_back(lineIndex);
					return true;
				});
			}

			std::sort(lineIndices.begin(), lineIndices.end());
			resultCtx.addLineIndices(lineIndices);
		}
	}

	CommandsRepo::Registry CommandsServer::genCommandsRegistry()
	{
		CommandsRepo::Registry registry;

		registry.tag = "Server";
		registry.registerCommandsCb = [](CommandsRepo::IRegisterCtx& registerCtx)
		{
			if (registerCtx.flavor() != FlavorsRepo::Type::WCSServer)
				return;

			registerCtx.registerCommand({ "Message", "All log lines pretending to the message", "message content",
				[](CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view params) { cmdMsg(resultCtx, linesTools, params); } });
		};

		return registry;
	}
}
