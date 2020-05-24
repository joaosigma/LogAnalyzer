#include "flavor_wcs_android_logcat.hpp"

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

				//skip process id
				{
					walker++; //ignore last space

					while ((walker < walkerEnd) && (walker[0] != '-'))
						walker++;

					if (walker >= walkerEnd)
						return false;
				}

				//parse threadId
				{
					walker++; //ignore '-'

					auto next = walker;
					while ((next < walkerEnd) && (next[0] >= '0') && (next[0] <= '9'))
						next++;

					if ((next >= walkerEnd) || (next[0] != '/'))
						return false;

					int32_t threadId;
					if (auto [p, ec] = std::from_chars(walker, next, threadId); ec != std::errc())
						return false;

					line.threadId = threadId;

					walker = next + 1; //ignore '/'
				}

				//skip app name
				{
					while ((walker < walkerEnd) && (walker[0] != ' '))
						walker++;

					if (walker >= walkerEnd)
						return false;
				}

				//parse level
				{
					if ((walker >= walkerEnd) || (walker[0] != ' '))
						return false;

					walker++;
					if (!FlavorsRepo::translateLogLevel(*walker, line.level))
						return false;

					while ((walker < walkerEnd) && (walker[0] != '/'))
						walker++;

					if (walker >= walkerEnd)
						return false;

					walker++; //skip "/"
				}
			}

			//we can now calculate each section but with a caveat: we either have a message from COMLib (with parameters) or we don't (it's a simple Android message)
			//to distinguish this, we check if we "have" an account, and if we do, then it's from COMLib

			if ((walkerEnd - walker) < 4)
				return false;

			auto haveACc = (walker[0] == '-') || (walker[0] == ' ') || ((walker[0] >= '0') && (walker[0] <= '9'));
			haveACc &= (walker[1] == '-') || (walker[1] == ' ') || ((walker[1] >= '0') && (walker[1] <= '9'));
			haveACc &= (walker[2] == '|');
			if (!haveACc)
			{
				//we have an Android message, which consists of only a tag and a message

				 //read the tag
				{
					line.sectionTag.offset = walker - line.data.start;
					line.sectionTag.size = 0;

					while ((walker < walkerEnd) && (walker[0] != ':'))
						walker++;

					line.sectionTag.size = walker - line.data.start - line.sectionTag.offset;

					walker++; //ignore ':'
				}

				//read the message
				{
					if ((walker >= walkerEnd) || (walker[0] != ' '))
						return false;

					walker++; //skip space
					line.sectionMsg.offset = walker - line.data.start;
					line.sectionMsg.size = walkerEnd - walker; //end of the line
				}
			}
			else
			{
				//we have a full COMLib message

				//skip the account info
				walker += 3;

				//read the tag
				{
					line.sectionTag.offset = walker - line.data.start;
					line.sectionTag.size = 0;

					while ((walker < walkerEnd) && (walker[0] != ':'))
						walker++;

					line.sectionTag.size = walker - line.data.start - line.sectionTag.offset;

					walker++; //ignore ':'
				}

				//read the method (starting from the tag)
				{
					if ((walker >= walkerEnd) || (walker[0] != ' '))
						return false;

					walker++; //skip space
					line.sectionMethod.offset = walker - line.data.start;
					line.sectionMethod.size = 0;

					while ((walker < walkerEnd) && (*walker != '|'))
						walker++;

					line.sectionMethod.size = walker - line.data.start - line.sectionMethod.offset;

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
					line.sectionMsg.offset = walker - line.data.start;
					line.sectionMsg.size = 0;

					while ((walker < walkerEnd) && (walker[0] != '|'))
						walker++;

					line.sectionMsg.size = walker - line.data.start - line.sectionMsg.offset;

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
					line.sectionParams.offset = walker - line.data.start;
					line.sectionParams.size = walkerEnd - walker; //end of the line
				}
			}

			//:)
			return true;
		}
	}

	FlavorsRepo::Info WCSAndroidLogCat::genFlavorsRepoInfo()
	{
		FlavorsRepo::Info info;
		info.parser = parse;
		return info;
	}
}
