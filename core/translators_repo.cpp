#include "translators_repo.hpp"

#include "translators/translator_wcs_comlib.hpp"

#include <array>

#include <nlohmann/json.hpp>

namespace la
{
	namespace
	{
		std::array< std::tuple<FlavorsRepo::Type, TranslatorsRepo::Translator>, 2> Translators{ {
				//known types
				{ FlavorsRepo::Type::WCSCOMLib, TranslatorCOMLib::genTranslator() },
				{ FlavorsRepo::Type::WCSAndroidLogcat, TranslatorCOMLib::genTranslator() },

				//any new types should be placed after this line
			} };

		template<class TCallback>
		size_t iterateParams(std::string_view params, TCallback&& cb)
		{
			if (params.empty())
				return 0;

			size_t numParams{ 0 };
			while (!params.empty())
			{
				//extract param name
				std::string_view paramName;
				{
					auto walker = params.data();
					auto walkerEnd = walker + params.size();
					while ((walker < walkerEnd) && (walker[0] != '='))
						walker++;

					if (walker >= walkerEnd) //weird: parameter didn't have a '=', so lets just assume that everything is just a name
					{
						numParams++;
						cb(params, std::string_view{});

						return numParams;
					}

					paramName = params.substr(0, walker - params.data());
					params = params.substr(paramName.size() + 1);
				}

				//extract param value
				std::string_view paramValue;
				if (!params.empty())
				{
					auto walker = params.data();
					auto walkerEnd = walker + params.size();

					//the value must end in "; " or "; EOF"
					while ((walker < walkerEnd))
					{
						while ((walker < walkerEnd) && (walker[0] != ';'))
							walker++;

						if ((walker >= walkerEnd) || (((walker + 1) < walkerEnd) && (walker[1] == ' ')))
							break;

						walker++;
					}

					if (walker >= walkerEnd)
					{
						paramValue = params;
						params = {};
					}
					else
					{
						paramValue = params.substr(0, walker - params.data());
						params = params.substr(paramValue.size() + 2);
					}
				}

				//one param...

				numParams++;
				if (!cb(paramName, paramValue))
					break;
			}

			return numParams;
		}
	}

	bool TranslatorsRepo::translate(Type type, Format format, FlavorsRepo::Type flavor, LogLine line, TranslationCtx& translationCtx)
	{
		assert(translationCtx.output.empty());

		if (line.data.end <= line.data.start)
			return false;

		if (type == Type::Raw)
		{
			//raw types never fail

			switch (format)
			{
			case Format::Line:
				translationCtx.output.append(line.data.start, line.data.end);
				return true;
			case Format::JSONFull:
			case Format::JSONSingleParams:
			{
				nlohmann::json jLine;

				jLine["timestamp"] = line.timestamp;
				jLine["threadId"] = line.threadId;
				jLine["level"] = line.level;

				jLine["tag"] = line.getSectionTag();
				jLine["method"] = line.getSectionMethod();
				jLine["msg"] = line.getSectionMsg();

				if (format == Format::JSONFull)
				{
					jLine["params"] = nlohmann::json::array();

					iterateParams(line.getSectionParams(), [&jParams = jLine["params"]](std::string_view paramName, std::string_view paramValue)
					{
						nlohmann::json jParam;
						jParam["name"] = paramName;
						jParam["value"] = paramValue;
						jParams.push_back(std::move(jParam));
						return true;
					});
				}
				else
				{
					jLine["params"] = line.getSectionParams();
				}

				translationCtx.output = jLine.dump(-1, '\t', false, nlohmann::detail::error_handler_t::replace);
				return true;
			}
			default:
				break;
			}

			return false;
		}
		else if (type == Type::Translated)
		{
			//reaching this point, we have to translate (we *can* more than one translator for the same flavor)

			for (const auto& [translatorFlavor, translator] : Translators)
			{
				if (translatorFlavor != flavor)
					continue;

				translationCtx.auxiliary.clear();
				translator.translate(line, translationCtx);
			}

			switch (format)
			{
			case Format::Line:

				if (translationCtx.output.empty())
					return TranslatorsRepo::translate(Type::Raw, format, flavor, line, translationCtx); //default to raw (no translation)
				return true;

			case Format::JSONFull:
			case Format::JSONSingleParams:
			{
				if (translationCtx.output.empty())
					return TranslatorsRepo::translate(Type::Raw, format, flavor, line, translationCtx); //default to raw (no translation)

				LogLine newLine;
				auto currentTranslation = translationCtx.output; //newLine will point to this string

				if (FlavorsRepo::processLineData(flavor, currentTranslation, newLine))
				{
					translationCtx.output.clear();
					translationCtx.auxiliary.clear();
					return TranslatorsRepo::translate(Type::Raw, format, flavor, newLine, translationCtx);
				}

				break;
			}
			default:
				break;
			}

			return false;
		}

		//unknown format
		return false;
	}
}
