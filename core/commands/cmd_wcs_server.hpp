#ifndef LA_CMD_WCS_SERVER_HPP
#define LA_CMD_WCS_SERVER_HPP

#include "../commands_repo.hpp"

namespace la
{
	class CommandsServer final
	{
	public:
		static CommandsRepo::Registry genCommandsRegistry();
	};
}

#endif
