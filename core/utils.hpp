#ifndef LA_UTILS_HPP
#define LA_UTILS_HPP

#include <tuple>
#include <iosfwd>
#include <string_view>

namespace la::utils
{
	struct Network
	{
		static bool writePCAPHeader(std::ostream& streamOut);
		static bool writePCAPDataIPV4(std::ostream& streamOut, std::string_view srcAddress, std::string_view dstAddress, int64_t timestamp, std::tuple<const void*, size_t> payload);
		static bool writePCAPDataIPV6(std::ostream& streamOut, std::string_view srcAddress, std::string_view dstAddress, int64_t timestamp, std::tuple<const void*, size_t> payload);
	};
}

#endif
