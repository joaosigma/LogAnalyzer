#ifndef LA_COMMANDS_REPO_HPP
#define LA_COMMANDS_REPO_HPP

#include "flavors_repo.hpp"

#include <string>
#include <functional>
#include <string_view>

#include <nlohmann/json_fwd.hpp>

namespace la
{
	class LinesTools;

	class CommandsRepo
	{
	public:
		struct IResultCtx
		{
			struct LineContent
			{
				size_t lineIndex;
				size_t contentOffset, contentSize;
			};

			virtual nlohmann::json& json() noexcept = 0;

			virtual void addNetworkPacketIPV4(std::string_view srcAddress, std::string_view dstAddress, int64_t timestamp, LineContent lineContent) = 0;
			virtual void addNetworkPacketIPV6(std::string_view srcAddress, std::string_view dstAddress, int64_t timestamp, LineContent lineContent) = 0;

			virtual size_t addLineIndices(std::string_view name, const std::vector<size_t>& indices) = 0;

			size_t addLineIndices(const std::vector<size_t>& indices)
			{
				return addLineIndices({}, indices);
			};
		};

		struct CommandInfo
		{
			std::string name;
			std::string help;
			std::string paramsHelp;
			bool supportLineExecution;

			std::function<void(IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view params)> executionCb;
		};

		struct IRegisterCtx
		{
			virtual FlavorsRepo::Type flavor() noexcept = 0;
			virtual void registerCommand(CommandInfo cmd) = 0;
		};

		struct Registry
		{
			std::string_view tag;
			std::function<void(IRegisterCtx& registerCtx)> registerCommandsCb;
		};

	public:
		static size_t iterateCommands(FlavorsRepo::Type flavor, const std::function<void(std::string_view tag, CommandsRepo::CommandInfo cmd)>& iterateCb);
	};
}

#endif
