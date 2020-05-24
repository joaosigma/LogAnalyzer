#ifndef LA_FLAVOR_WCS_SERVER_HPP
#define LA_FLAVOR_WCS_SERVER_HPP

#include "../flavors_repo.hpp"

namespace la::flavors
{
	class WCSServer
	{
	public:
		static FlavorsRepo::Info genFlavorsRepoInfo();
	};
}

#endif
