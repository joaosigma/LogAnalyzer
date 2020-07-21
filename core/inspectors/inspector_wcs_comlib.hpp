#ifndef LA_INSPECTORS_WCS_COMLIB_HPP
#define LA_INSPECTORS_WCS_COMLIB_HPP

#include "../inspectors_repo.hpp"

namespace la
{
	class InspectorsCOMLib final
	{
	public:
		static InspectorsRepo::Registry genInspectorRegistry();
	};
}

#endif
