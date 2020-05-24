#include "log_line.hpp"

#include <cassert>

namespace la
{
	bool LogLine::paramExtract(std::string_view param, std::string_view& value) const noexcept
	{
		if (param.empty())
			return false;

		std::string_view params = { data.start + sectionParams.offset, sectionParams.size };

		while (!params.empty())
		{
			//found param in parameters, just have to read the value
			if (((params.size() + 1) >= param.size()) && (params.compare(0, param.size(), param) == 0) && (params[param.size()] == '='))
			{
				//isolate the value into params
				{
					params = params.substr(param.size() + 1);

					auto walker = params.data();
					auto walkerEnd = walker + params.size();

					//the value must end in "; " or "; EOF"
					while ((walker < walkerEnd))
					{
						while ((walker < walkerEnd) && (walker[0] != ';'))
							walker++;

						if (walker >= walkerEnd)
							return false;

						if (((walker + 1) < walkerEnd) && (walker[1] == ' '))
							break;
					}

					if (walker >= walkerEnd)
						return false;

					params = params.substr(0, walker - params.data());
				}

				//this is it
				value = params;
				return true;
			}

			//move to next param in parameters
			{
				auto walker = params.data();
				auto walkerEnd = walker + params.size();

				//the parameter must end in "; " (if there's a next one)
				while ((walker < walkerEnd))
				{
					while ((walker < walkerEnd) && (walker[0] != ';'))
						walker++;

					if (walker >= walkerEnd)
						return false;

					walker++; //skip ';'
					if (walker >= walkerEnd)
						return false;

					if (walker[0] == ' ')
						break;
				}

				assert(walker[0] == ' ');
				params = params.substr(walker - params.data() + 1);
			}
		}

		return false;
	}
}
