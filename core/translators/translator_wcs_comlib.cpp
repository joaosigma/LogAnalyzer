#include "translator_wcs_comlib.hpp"

#include <regex>
#include <cassert>
#include <charconv>
#include <type_traits>

namespace la
{
    namespace
    {
        template<class TMatchType, class TMatchTranslator>
        bool doMatchParams(LogLine line, const std::regex& filter, TMatchTranslator&& matchTranslator, TranslatorsRepo::TranslationCtx& translationCtx)
        {
            static_assert(std::is_same_v<TMatchType, std::string_view> || std::is_arithmetic_v<TMatchType>);

            std::cmatch match;
            if (translationCtx.output.empty())
            {
                auto params = line.getSectionParams();
                if (!std::regex_search(params.data(), params.data() + params.size(), match, filter) || (match.size() != 2))
                    return false;
            }
            else
            {
                assert(line.sectionParams.offset < translationCtx.output.size());

                const char* start = translationCtx.output.data() + line.sectionParams.offset;
                const char* end = start + translationCtx.output.size() - line.sectionParams.offset;
                if (!std::regex_search(start, end, match, filter) || (match.size() != 2))
                    return false;
            }

            std::string_view newValue;
            if constexpr (std::is_same_v<TMatchType, std::string_view>)
            {
                newValue = matchTranslator(std::string_view{ match[1].first, static_cast<size_t>(match[1].second - match[1].first) });
                if (newValue.empty())
                    return false;
            }
            else
            {
                TMatchType value;
                if (auto [p, ec] = std::from_chars(match[1].first, match[1].second, value); ec != std::errc())
                    return false;

                newValue = matchTranslator(value);
                if (newValue.empty())
                    return false;
            }

            if (translationCtx.output.empty())
            {
                translationCtx.output.append(line.data.start, line.data.start + line.sectionParams.offset);

                translationCtx.output.append(match.prefix().first, match.prefix().second);
                translationCtx.output
                    .append(match[0].first, match[1].first - match[0].first)
                    .append(newValue)
                    .append(match[1].second, match[0].second - match[1].second);
                translationCtx.output.append(match.suffix().first, match.suffix().second);
            }
            else
            {
                translationCtx.auxiliary.clear();
                translationCtx.auxiliary.append(line.data.start, line.data.start + line.sectionParams.offset);

                translationCtx.auxiliary.append(match.prefix().first, match.prefix().second);
                translationCtx.auxiliary
                    .append(match[0].first, match[1].first - match[0].first)
                    .append(newValue)
                    .append(match[1].second, match[0].second - match[1].second);
                translationCtx.auxiliary.append(match.suffix().first, match.suffix().second);

                std::swap(translationCtx.auxiliary, translationCtx.output);
            }

            return true;
        }

        std::regex RegXFilterType{ R"(; type=(\d+);)", std::regex::ECMAScript | std::regex::optimize | std::regex::icase };
        std::regex RegXFilterState{ R"(; state=(\d+);)", std::regex::ECMAScript | std::regex::optimize | std::regex::icase };

        bool translateMessageState(LogLine line, TranslatorsRepo::TranslationCtx& translationCtx)
        {
            auto validLine = line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.ChatController") && (
                (line.checkSectionMethod<LogLine::MatchType::EndsWith>("storeMessage") && (line.checkSectionMsg<LogLine::MatchType::Exact>("Storing message") || line.checkSectionMsg<LogLine::MatchType::Exact>("Message Stored"))) ||
                (line.checkSectionMethod<LogLine::MatchType::EndsWith>("onMessageHandled") && line.checkSectionMsg<LogLine::MatchType::Exact>("Message Handled")) ||
                (line.checkSectionMethod<LogLine::MatchType::EndsWith>("onUpdateMessageData") && line.checkSectionMsg<LogLine::MatchType::Exact>("Message state updated")) ||
                (line.checkSectionMethod<LogLine::MatchType::EndsWith>("onUpdateMessageState") && line.checkSectionMsg<LogLine::MatchType::Exact>("Message state updated")) ||
                (line.checkSectionMethod<LogLine::MatchType::EndsWith>("onSendPendingMessagesCompleted") && line.checkSectionMsg<LogLine::MatchType::Exact>("Send pending messages completed")) ||
                (line.checkSectionMethod<LogLine::MatchType::EndsWith>("onNotificationResponse") && line.checkSectionMsg<LogLine::MatchType::Exact>("Message Notification Response")));
            validLine = validLine || (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.Sync.CMSProducer") && line.checkSectionMethod<LogLine::MatchType::EndsWith>("processNewIMDN") && (
                line.checkSectionMsg<LogLine::MatchType::Exact>("message notified") || line.checkSectionMsg<LogLine::MatchType::Exact>("message state updated") || line.checkSectionMsg<LogLine::MatchType::Exact>("sending notification")));
            validLine = validLine || (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.GroupChatController") && line.checkSectionMethod<LogLine::MatchType::EndsWith>("onGroupChatMessageUpdated") && line.checkSectionMsg<LogLine::MatchType::Exact>("Chat message updated"));
            validLine = validLine || (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.FileTransferController.HTTPFileTransfer") && line.checkSectionMethod<LogLine::MatchType::EndsWith>("onChatMessageSynced") && line.checkSectionMsg<LogLine::MatchType::Exact>(""));
            validLine = validLine || (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.ChatController.SMSoIP") && (
                (line.checkSectionMethod<LogLine::MatchType::EndsWith>("sendNotification") && line.checkSectionMsg<LogLine::MatchType::Exact>("sending notification")) ||
                (line.checkSectionMethod<LogLine::MatchType::EndsWith>("handleIncomingNotification") && line.checkSectionMsg<LogLine::MatchType::Exact>("Incoming notification"))));
            validLine = validLine || (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.ChatController.SMS") && line.checkSectionMethod<LogLine::MatchType::EndsWith>("sendNotification") && line.checkSectionMsg<LogLine::MatchType::Exact>("Sending message"));
            validLine = validLine || (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.ChatController.RCS") && line.checkSectionMethod<LogLine::MatchType::EndsWith>("onMessageState") && line.checkSectionMsg<LogLine::MatchType::Exact>("onMessageState"));
            validLine = validLine || (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.ChatController.OMASIMPLEIM") && line.checkSectionMethod<LogLine::MatchType::EndsWith>("sendNotification") && line.checkSectionMsg<LogLine::MatchType::Exact>("Sending Notification"));
            validLine = validLine || (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.ChatController.CPM") && line.checkSectionMethod<LogLine::MatchType::EndsWith>("sendNotification") && line.checkSectionMsg<LogLine::MatchType::Exact>("sending Notification"));
            validLine = validLine || (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.LegacyStandalone") && line.checkSectionMethod<LogLine::MatchType::EndsWith>("sendNotification") && line.checkSectionMsg<LogLine::MatchType::Exact>("sending notification"));
            validLine = validLine || (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.ChatController.OMACPMStandalone") && (
                (line.checkSectionMethod<LogLine::MatchType::EndsWith>("sendNotification") && line.checkSectionMsg<LogLine::MatchType::Exact>("sending notification")) ||
                (line.checkSectionMethod<LogLine::MatchType::EndsWith>("onIncomingCPMNotification") && line.checkSectionMsg<LogLine::MatchType::Exact>("Incoming CPM standalone notification"))));
            validLine = validLine || (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.Chatbot") && line.checkSectionMethod<LogLine::MatchType::EndsWith>("onMessageUpdated") && line.checkSectionMsg<LogLine::MatchType::Exact>("Checking pending delete token operations"));

            if (!validLine)
                return false;

            return doMatchParams<int8_t>(line, RegXFilterState, [](int8_t value) -> std::string_view
            {
                switch (value)
                {
                case 0: return "none";
                case 1: return "pending";
                case 2: return "sending";
                case 3: return "sent";
                case 4: return "received";
                case 5: return "failed";
                case 6: return "delivered";
                case 7: return "displayed";
                default: return "unknown";
                }
            }, translationCtx);
        }

        bool translateGCInfo(LogLine line, TranslatorsRepo::TranslationCtx& translationCtx)
        {
            if (!line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.GroupChatController"))
                return false;

            bool hasBoth = line.checkSectionMsg<LogLine::MatchType::Exact>("storing updated gc info");
            bool hasType = hasBoth ? true : line.checkSectionMsg<LogLine::MatchType::Exact>("storing new gc info");

            bool success{ false };

            if (hasBoth || hasType)
            {
                success |= doMatchParams<int8_t>(line, RegXFilterType, [](int8_t value) -> std::string_view
                {
                    switch (value)
                    {
                    case 0: return "none";
                    case 1: return "rcs";
                    case 2: return "broadcast";
                    case 3: return "groupMMS";
                    default: return "unknown";
                    }
                }, translationCtx);
            }

            if (hasBoth)
            {
                success |= doMatchParams<int8_t>(line, RegXFilterState, [](int8_t value) -> std::string_view
                {
                    switch (value)
                    {
                    case 0: return "none";
                    case 1: return "inviting";
                    case 2: return "invited";
                    case 3: return "connecting";
                    case 4: return "connected";
                    case 5: return "disconnected";
                    case 6: return "closed";
                    default: return "unknown";
                    }
                }, translationCtx);
            }

            return success;
        }
    }

    TranslatorsRepo::Translator TranslatorCOMLib::genTranslator()
    {
        TranslatorsRepo::Translator translator;
        translator.translate = [](LogLine line, TranslatorsRepo::TranslationCtx& translationCtx) -> bool
        {
            //these translations are mutually exclusive
            if (translateMessageState(line, translationCtx))
                return true;
            if (translateGCInfo(line, translationCtx))
                return true;
            return false;
        };

        return translator;
    }
}
