#ifndef LA_CMD_WCS_UI_ANDROID_HPP
#define LA_CMD_WCS_UI_ANDROID_HPP

#include "../commands_repo.hpp"

namespace la
{
	class CommandsUIAndroid final
	{
	public:
		static CommandsRepo::Registry genCommandsRegistry();
	};
}

#endif
