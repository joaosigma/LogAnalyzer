#ifndef LA_CMD_WCS_COMLIB_HPP
#define LA_CMD_WCS_COMLIB_HPP

#include "../commands_repo.hpp"

namespace la
{
	class CommandsCOMLib final
	{
	public:
		static CommandsRepo::Registry genCommandsRegistry();
	};
}

#endif
