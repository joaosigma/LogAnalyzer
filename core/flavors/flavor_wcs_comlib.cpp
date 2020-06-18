#include "flavor_wcs_comlib.hpp"

#include "../log_line.hpp"
#include "../flavors_repo.hpp"

#include <charconv>

namespace la::flavors
{
	namespace
	{
		bool parse(LogLine& line)
		{
			auto walker = line.data.start;
			auto walkerEnd = line.data.end;

			//parse and convert the initial data (timestamp, level, etc.)
			{
				if ((walker >= walkerEnd) || ((walkerEnd - walker) < 24))
					return false;
				if ((walker[4] != '-') || (walker[7] != '-') || (walker[10] != ' ') || (walker[13] != ':') || (walker[16] != ':') || (walker[19] != '.') || (walker[23] != ' '))
					return false;

				//parse timestamp
				{
					line.timestamp = FlavorsRepo::translateTimestamp(walker);
					if (line.timestamp == 0)
						return false;

					walker += 23;
					if (walker[0] != ' ')
						return false;
				}

				//parse threadId
				{
					walker++; //ignore last space

					auto next = walker;
					while ((next < walkerEnd) && (next[0] >= '0') && (next[0] <= '9'))
						next++;

					if ((next >= walkerEnd) || (next[0] != ' '))
						return false;

					int32_t threadId;
					if (auto [p, ec] = std::from_chars(walker, next, threadId); ec != std::errc())
						return false;

					line.threadId = threadId;

					walker = next + 1; //ignore last space
				}

				//parse level
				{
					if ((walker >= walkerEnd) || (walker[0] != '|'))
						return false;

					walker++;
					if (!FlavorsRepo::translateLogLevel(*walker, line.level))
						return false;

					while ((walker < walkerEnd) && (walker[0] != '|'))
						walker++;

					if (walker >= walkerEnd)
						return false;

					walker++; //skip "|"
				}

				//skip account
				{
					while ((walker < walkerEnd) && (walker[0] != '|'))
						walker++;

					if (walker >= walkerEnd)
						return false;

					walker++; //ignore '|'
				}
			}

			//we can now calculate each section

			//read the tag
			{
				line.sectionTag.offset = static_cast<uint16_t>(walker - line.data.start);
				line.sectionTag.size = 0;

				while ((walker < walkerEnd) && (walker[0] != ':'))
					walker++;

				line.sectionTag.size = static_cast<uint32_t>(walker - line.data.start - line.sectionTag.offset);

				walker++; //ignore ':'
			}

			//read the method (starting from the tag)
			{
				if ((walker >= walkerEnd) || (walker[0] != ' '))
					return false;

				walker++; //skip space
				line.sectionMethod.offset = static_cast<uint16_t>(walker - line.data.start);
				line.sectionMethod.size = 0;

				while ((walker < walkerEnd) && (*walker != '|'))
					walker++;

				line.sectionMethod.size = static_cast<uint32_t>(walker - line.data.start - line.sectionMethod.offset);

				if ((line.sectionMethod.size < 1) || (walker[-1] != ' ')) //last caracter before '|' must be a space
					return false;

				line.sectionMethod.size--; //ignore last space
				walker++; //ignore '|'
			}

			//read the message (starting from the method)
			{
				if ((walker >= walkerEnd) || (walker[0] != ' '))
					return false;

				walker++; //skip space
				line.sectionMsg.offset = static_cast<uint16_t>(walker - line.data.start);
				line.sectionMsg.size = 0;

				while ((walker < walkerEnd) && (walker[0] != '|'))
					walker++;

				line.sectionMsg.size = static_cast<uint32_t>(walker - line.data.start - line.sectionMsg.offset);

				if (walker < walkerEnd) //the message *can* be the last thing in the line
				{
					if ((line.sectionMsg.size < 1) || (walker[-1] != ' ')) //last caracter before '|' must be a space
						return false;

					line.sectionMsg.size--; //ignore last space
					walker++; //ignore '|'
				}
			}

			//read the params (starting from the message)
			if (walker < walkerEnd)
			{
				if (walker[0] != ' ')
					return false;

				walker++;
				line.sectionParams.offset = static_cast<uint16_t>(walker - line.data.start);
				line.sectionParams.size = static_cast<uint32_t>(walkerEnd - walker); //end of the line
			}

			//:)
			return true;
		}
	}

	FlavorsRepo::Info WCSCOMLib::genFlavorsRepoInfo()
	{
		FlavorsRepo::Info info;
		info.parser = parse;
		info.filesFilter.filter = R"(comlib\.\d\d\d\.log)";
		info.filesFilter.filterSort = R"(comlib\.(\d\d\d)\.log)";
		info.filesFilter.reverseSort = true;
		info.filesFilter.lineIdentification = R"(^\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d.\d\d\d \d+ \|[A-Z]{4,} ?\|[\-0-9]{2}\|\w)";

		return info;
	}
}
