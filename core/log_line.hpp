#ifndef LA_LOG_LINE_HPP
#define LA_LOG_LINE_HPP

#include <string>
#include <cstdint>
#include <charconv>
#include <string_view>
#include <type_traits>

namespace la
{
	enum class LogLevel : uint8_t { Trace, Debug, Info, Warn, Error, Fatal };

	struct LogLine
	{
		enum class MatchType : uint8_t { Exact, StartsWith, EndsWith, Contains };

		int32_t id;

		LogLevel level;
		int32_t threadId;
		int64_t timestamp;

		struct {
			const char* start;
			const char* end;

			bool empty() const noexcept
			{
				return (end == start);
			}

			size_t size() const noexcept
			{
				return (end - start);
			}

		} data;

		struct {
			uint16_t offset;
			uint32_t size;
		} sectionThreadName, sectionTag, sectionMethod, sectionMsg, sectionParams;

		std::string_view toStr() const noexcept { return { data.start, static_cast<size_t>(data.end - data.start) }; }

		std::string_view getSectionThreadName() const noexcept { return { data.start + sectionThreadName.offset, sectionThreadName.size }; }
		std::string_view getSectionTag() const noexcept { return { data.start + sectionTag.offset, sectionTag.size }; }
		std::string_view getSectionMethod() const noexcept { return { data.start + sectionMethod.offset, sectionMethod.size }; }
		std::string_view getSectionMsg() const noexcept { return { data.start + sectionMsg.offset, sectionMsg.size }; }
		std::string_view getSectionParams() const noexcept { return { data.start + sectionParams.offset, sectionParams.size }; }

		bool paramExtract(std::string_view param, std::string_view& value) const noexcept;

		template<class T>
		bool paramExtractAs(std::string_view param, T& value) const
		{
			static_assert(std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string> || std::is_arithmetic_v<T>);

			std::string_view valueStr;
			if (!paramExtract(param, valueStr))
				return false;

			if constexpr (std::is_same_v<T, std::string_view>)
			{
				value = valueStr;
				return true;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				value = std::string{ valueStr };
				return true;
			}
			else
			{
				auto [p, ec] = std::from_chars(valueStr.data(), valueStr.data() + valueStr.size(), value);
				return  (ec == std::errc());
			}
		}

		template<class T>
		bool paramCheck(std::string_view param, const T& value) const
		{
			T paramValue;
			if (!paramExtractAs<T>(param, paramValue))
				return false;

			return (paramValue == value);
		}

		template<MatchType TMatchType>
		bool checkSectionThreadName(std::string_view name) const noexcept
		{
			std::string_view curName{ data.start + sectionThreadName.offset, sectionThreadName.size };

			if constexpr (TMatchType == MatchType::Exact)
				return (curName == name);
			else if constexpr (TMatchType == MatchType::StartsWith)
				return ((curName.size() >= name.size()) && (curName.compare(0, name.size(), name) == 0));
			else if constexpr (TMatchType == MatchType::EndsWith)
				return ((name.size() > curName.size()) ? false : (curName.compare(curName.length() - name.length(), name.length(), name) == 0));
			else if constexpr (TMatchType == MatchType::Contains)
				return (curName.find(name) != std::string_view::npos);
			else
				return false;
		}

		template<MatchType TMatchType>
		bool checkSectionTag(std::string_view tag) const noexcept
		{
			std::string_view curTag{ data.start + sectionTag.offset, sectionTag.size };

			if constexpr (TMatchType == MatchType::Exact)
				return (curTag == tag);
			else if constexpr (TMatchType == MatchType::StartsWith)
				return ((curTag.size() >= tag.size()) && (curTag.compare(0, tag.size(), tag) == 0));
			else if constexpr (TMatchType == MatchType::EndsWith)
				return ((tag.size() > curTag.size()) ? false : (curTag.compare(curTag.length() - tag.length(), tag.length(), tag) == 0));
			else if constexpr (TMatchType == MatchType::Contains)
				return (curTag.find(tag) != std::string_view::npos);
			else
				return false;
		}

		template<MatchType TMatchType>
		bool checkSectionMethod(std::string_view method) const noexcept
		{
			std::string_view curMethod{ data.start + sectionMethod.offset, sectionMethod.size };

			if constexpr (TMatchType == MatchType::Exact)
				return (curMethod == method);
			else if constexpr (TMatchType == MatchType::StartsWith)
				return ((curMethod.size() >= method.size()) && (curMethod.compare(0, method.size(), method) == 0));
			else if constexpr (TMatchType == MatchType::EndsWith)
				return ((method.size() > curMethod.size()) ? false : (curMethod.compare(curMethod.length() - method.length(), method.length(), method) == 0));
			else if constexpr (TMatchType == MatchType::Contains)
				return (curMethod.find(method) != std::string_view::npos);
			else
				return false;
		}

		template<MatchType TMatchType>
		bool checkSectionMsg(std::string_view msg) const noexcept
		{
			std::string_view curMsg{ data.start + sectionMsg.offset, sectionMsg.size };

			if constexpr (TMatchType == MatchType::Exact)
				return (curMsg == msg);
			else if constexpr (TMatchType == MatchType::StartsWith)
				return ((curMsg.size() >= msg.size()) && (curMsg.compare(0, msg.size(), msg) == 0));
			else if constexpr (TMatchType == MatchType::EndsWith)
				return ((msg.size() > curMsg.size()) ? false : (curMsg.compare(curMsg.length() - msg.length(), msg.length(), msg) == 0));
			else if constexpr (TMatchType == MatchType::Contains)
				return (curMsg.find(msg) != std::string_view::npos);
			else
				return false;
		}

		template<MatchType TMatchType>
		bool checkSectionParams(std::string_view params) const noexcept
		{
			std::string_view curParams{ data.start + sectionParams.offset, sectionParams.size };

			if constexpr (TMatchType == MatchType::Exact)
				return (curParams == params);
			else if constexpr (TMatchType == MatchType::StartsWith)
				return ((curParams.size() >= params.size()) && (curParams.compare(0, params.size(), params) == 0));
			else if constexpr (TMatchType == MatchType::EndsWith)
				return ((params.size() > curParams.size()) ? false : (curParams.compare(curParams.length() - params.length(), params.length(), params) == 0));
			else if constexpr (TMatchType == MatchType::Contains)
				return (curParams.find(params) != std::string_view::npos);
			else
				return false;
		}
	};

	static_assert(std::is_trivial_v<LogLine>);
}

#endif
