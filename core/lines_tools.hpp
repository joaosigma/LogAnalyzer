#ifndef LA_LINES_TOOLS_HPP
#define LA_LINES_TOOLS_HPP

#include "log_line.hpp"

#include <vector>
#include <optional>
#include <functional>
#include <string_view>

namespace la
{
	class LinesTools
	{
	public:
		enum class FilterType : int8_t { LogLevel, ThreadId, ThreadName, Tag, Method, Msg };

		template<FilterType TFilterType, class TFilterValue, LogLine::MatchType TFilterValueMatchType = LogLine::MatchType::Exact>
		class FilterParam;

		template<>
		class FilterParam<FilterType::LogLevel, LogLevel, LogLine::MatchType::Exact>
		{
		public:
			constexpr FilterParam(LogLevel value) noexcept
				: m_value{ value }
			{ }

			constexpr bool operator()(const LogLine& line) const noexcept { return (line.level == m_value); }

		private:
			LogLevel m_value;
		};

		template<>
		class FilterParam<FilterType::ThreadId, int32_t, LogLine::MatchType::Exact>
		{
		public:
			constexpr FilterParam(int32_t value) noexcept
				: m_value{ value }
			{ }

			constexpr bool operator()(const LogLine& line) const noexcept { return (line.threadId == m_value); }

		private:
			int32_t m_value;
		};

		template<FilterType TFilterType, LogLine::MatchType TFilterValueMatchType>
		class FilterParam<TFilterType, std::string_view, TFilterValueMatchType>
		{
			static_assert((TFilterType == FilterType::ThreadName) || (TFilterType == FilterType::Tag) || (TFilterType == FilterType::Method) || (TFilterType == FilterType::Msg));
			static_assert((TFilterValueMatchType == LogLine::MatchType::Exact) || (TFilterValueMatchType == LogLine::MatchType::StartsWith) || (TFilterValueMatchType == LogLine::MatchType::EndsWith) || (TFilterValueMatchType == LogLine::MatchType::Contains));

		public:
			constexpr FilterParam(std::string_view value) noexcept
				: m_value{ value }
			{ }

			constexpr bool operator()(const LogLine& line) const noexcept
			{
				if constexpr (TFilterType == FilterType::ThreadName)
					return line.checkSectionThreadName<TFilterValueMatchType>(m_value);
				else if constexpr (TFilterType == FilterType::Tag)
					return line.checkSectionTag<TFilterValueMatchType>(m_value);
				else if constexpr (TFilterType == FilterType::Method)
					return line.checkSectionMethod<TFilterValueMatchType>(m_value);
				else if constexpr (TFilterType == FilterType::Msg)
					return line.checkSectionMsg<TFilterValueMatchType>(m_value);
				else
					return false;
			}

		private:
			std::string_view m_value;
		};

		template <class... TParams>
		class FilterCollection
		{
		public:
			constexpr FilterCollection(TParams&&... params)
				: m_params(std::forward<TParams>(params)...)
			{ }

			constexpr bool operator()(const la::LogLine& line) const noexcept
			{
				return std::apply([&line](auto... param) { return (param(line) && ...); }, m_params);
			}

		private:
			std::tuple<TParams...> m_params;
		};

	public:
		struct LineIndexRange
		{
			size_t start{ 0 }, end{ 0 };

			constexpr bool empty() const noexcept
			{
				return (start >= end);
			}

			constexpr size_t numLines() const noexcept
			{
				return ((start < end) ? (end - start) : 0);
			}
		};

		struct SearchResult
		{
			bool valid{ false };
			size_t lineIndex{ 0 };
			size_t lineOffset{ 0 };
		};

	public:
		LinesTools(const std::vector<LogLine>& lines) noexcept
			: m_lines{ lines }
		{ }

		const std::vector<LogLine>& lines() const;

		template<class TFilterCb, class... TParams>
		size_t windowIterate(LineIndexRange targetRange, FilterCollection<TParams...> filter, TFilterCb&& filterCb) const
		{
			static_assert(std::is_invocable_r_v<bool, TFilterCb, size_t, LogLine, size_t>);

			if ((targetRange.start >= targetRange.end) || (targetRange.start >= m_lines.size()))
				return 0;

			if (targetRange.end > m_lines.size())
				targetRange.end = m_lines.size();

			size_t curIndex{ 0 };
			size_t linesProcessed{ 0 };
			while (targetRange.start < targetRange.end)
			{
				linesProcessed++;

				const auto& line = m_lines[targetRange.start];
				if (filter(line) && !filterCb(curIndex++, line, targetRange.start))
					break;

				targetRange.start++;
			}

			return linesProcessed;
		}

		SearchResult windowSearch(LineIndexRange targetRange, size_t startCharacterIndex, const std::function<const char* (const char*, const char*)>& cbSearch) const;

		std::vector<size_t> windowFindAll(LineIndexRange targetRange, std::string_view contentQuery) const;
		std::optional<size_t> windowFindFirst(LineIndexRange targetRange, std::string_view contentQuery) const;

		template<class TFilterCb, class... TParams>
		size_t iterateBackwards(size_t lineIndexStart, FilterCollection<TParams...> filter, TFilterCb&& filterCb) const
		{
			static_assert(std::is_invocable_r_v<bool, TFilterCb, size_t, LogLine, size_t>);

			size_t curIndex{ 0 };
			size_t linesProcessed{ 0 };
			while (true)
			{
				linesProcessed++;

				auto& line = m_lines[lineIndexStart];
				if (filter(line) && !filterCb(curIndex++, line, lineIndexStart))
					break;

				if (lineIndexStart == 0)
					break;
				lineIndexStart--;
			}

			return linesProcessed;
		}

		template<class TFilterCb, class... TParams>
		size_t iterateForward(size_t lineIndexStart, FilterCollection<TParams...> filter, TFilterCb&& filterCb) const
		{
			static_assert(std::is_invocable_r_v<bool, TFilterCb, size_t, LogLine, size_t>);

			size_t curIndex{ 0 };
			size_t linesProcessed{ 0 };
			while (lineIndexStart < m_lines.size())
			{
				linesProcessed++;

				auto& line = m_lines[lineIndexStart];
				if (filter(line) && !filterCb(curIndex++, line, lineIndexStart))
					break;

				lineIndexStart++;
			}

			return linesProcessed;
		}

	private:
		const std::vector<LogLine>& m_lines;
	};
}

#endif
