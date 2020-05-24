#ifndef LA_TRANSLATOR_WCS_COMLIB_HPP
#define LA_TRANSLATOR_WCS_COMLIB_HPP

#include "../translators_repo.hpp"

namespace la
{
    class TranslatorCOMLib final
    {
    public:
        static TranslatorsRepo::Translator genTranslator();
    };
}

#endif
