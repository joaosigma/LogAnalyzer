#include "commands_repo.hpp"

#include "commands/cmd_wcs_comlib.hpp"
#include "commands/cmd_wcs_server.hpp"

#include <array>

namespace la
{
	namespace
	{
		std::array<CommandsRepo::Registry, 2> CommandsRegistry{ {
				//known types
				{ CommandsCOMLib::genCommandsRegistry() },
				{ CommandsServer::genCommandsRegistry() },

				//any new types should be placed after this line
			} };
	}

	size_t CommandsRepo::iterateCommands(FlavorsRepo::Type flavor, const std::function<void(std::string_view tag, CommandsRepo::CommandInfo cmd)>& iterateCb)
	{
		class Context final
			: public IRegisterCtx
		{
		public:
			Context(FlavorsRepo::Type flavor, const std::function<void(std::string_view tag, CommandsRepo::CommandInfo cmd)>& iterateCb)
				: m_flavor{ flavor }
				, m_iterateCb{ iterateCb }
			{ }

			size_t count() const noexcept
			{
				return m_count;
			}

			void updateTag(std::string_view tag) noexcept
			{
				m_tag = tag;
			}

			FlavorsRepo::Type flavor() noexcept override
			{
				return m_flavor;
			}

			void registerCommand(CommandInfo cmd) override
			{
				if (!cmd.executionCb || cmd.name.empty())
					return;

				m_count++;
				m_iterateCb(m_tag, std::move(cmd));
			}

		private:
			size_t m_count{ 0 };
			std::string_view m_tag;
			FlavorsRepo::Type m_flavor;
			const std::function<void(std::string_view tag, CommandsRepo::CommandInfo cmd)>& m_iterateCb;
		};

		Context ctx{ flavor, iterateCb };

		for (const auto& reg : CommandsRegistry)
		{
			if (reg.tag.empty())
				continue;

			ctx.updateTag(reg.tag);
			reg.registerCommandsCb(ctx);
		}

		return ctx.count();
	}
}
