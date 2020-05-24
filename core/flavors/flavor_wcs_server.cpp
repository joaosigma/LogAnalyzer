#include "flavor_wcs_server.hpp"

#include "../log_line.hpp"
#include "../flavors_repo.hpp"

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
				if ((walker[4] != '-') || (walker[7] != '-') || (walker[10] != ' ') || (walker[13] != ':') || (walker[16] != ':') || (walker[19] != '.') || (walker[23] != '|'))
					return false;

				//parse timestamp
				{
					line.timestamp = FlavorsRepo::translateTimestamp(walker);
					if (line.timestamp == 0)
						return false;

					walker += 23;
					if (walker[0] != '|')
						return false;
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
			}

			//calculate each section

			//read the thread name
			{
				line.sectionThreadName.offset = walker - line.data.start;
				line.sectionThreadName.size = 0;

				while ((walker < walkerEnd) && (walker[0] != '|'))
					walker++;

				if (walker >= walkerEnd)
					return false;

				line.sectionThreadName.size = walker - line.data.start - line.sectionThreadName.offset;

				walker++; //ignore '|'
			}

			//read the tag
			{
				line.sectionTag.offset = walker - line.data.start;
				line.sectionTag.size = 0;

				while ((walker < walkerEnd) && (walker[0] != '|'))
					walker++;

				line.sectionTag.size = walker - line.data.start - line.sectionTag.offset;

				walker++; //ignore '|'
			}

			//read the method
			{
				line.sectionMethod.offset = walker - line.data.start;
				line.sectionMethod.size = 0;

				while ((walker < walkerEnd) && (walker[0] != '|'))
					walker++;

				line.sectionMethod.size = walker - line.data.start - line.sectionMethod.offset;

				walker++; //ignore '|'
			}

			//read the message
			{
				line.sectionMsg.offset = walker - line.data.start;
				line.sectionMsg.size = walkerEnd - walker; //end of the line
			}

			//trim any spaces at the end of each section
			{
				for (auto walker = line.data.start + line.sectionThreadName.offset + line.sectionThreadName.size - 1; (line.sectionThreadName.size > 0) && (walker[0] == ' '); line.sectionThreadName.size--, walker--);
				for (auto walker = line.data.start + line.sectionTag.offset + line.sectionTag.size - 1; (line.sectionTag.size > 0) && (walker[0] == ' '); line.sectionTag.size--, walker--);
				for (auto walker = line.data.start + line.sectionMethod.offset + line.sectionMethod.size - 1; (line.sectionMethod.size > 0) && (walker[0] == ' '); line.sectionMethod.size--, walker--);
			}

			//:)
			return true;
		}
	}

	FlavorsRepo::Info WCSServer::genFlavorsRepoInfo()
	{
		FlavorsRepo::Info info;
		info.parser = parse;
		info.filesFilter.filter = R"(\d\d-(console|msrp|sip|libs|cms)\.log)";
		info.filesFilter.filterSort = R"((\d\d)-(?:console|msrp|sip|libs|cms)\.log)";
		info.filesFilter.reverseSort = true;
		info.filesFilter.lineIdentification = R"(^\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d.\d\d\d\|[A-Z]{4,} ?\|[\w -]+\|\w+)";

		return info;
	}
}
