#ifndef LA_TRANSLATORS_REPO_HPP
#define LA_TRANSLATORS_REPO_HPP

#include "log_line.hpp"
#include "flavors_repo.hpp"

#include <string>

namespace la
{
	class TranslatorsRepo
	{
	public:
		enum class Type : uint8_t
		{
			Raw,
			Translated,
		};

		enum class Format : uint8_t
		{
			Line,
			JSONFull,
			JSONSingleParams,
		};

		struct TranslationCtx
		{
			std::string output;
			std::string auxiliary;
		};

		struct Translator
		{
			std::function<bool(LogLine line, TranslationCtx& translationCtx)> translate;
		};

	public:
		static bool translate(Type type, Format format, FlavorsRepo::Type flavor, LogLine line, TranslationCtx& translationCtx);
	};
}

#endif
