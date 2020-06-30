#ifndef LA_LINES_REPO_HPP
#define LA_LINES_REPO_HPP

#include "log_line.hpp"
#include "lines_tools.hpp"
#include "flavors_repo.hpp"
#include "commands_repo.hpp"
#include "translators_repo.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>

namespace la
{
	class FilesRepo;

	class LinesRepo final
	{
	public:
		struct ExportOptions
		{
			std::string_view filePath;
			bool appendToFile{ true };

			TranslatorsRepo::Type translationType{ TranslatorsRepo::Type::Raw };
			TranslatorsRepo::Format translationFormat{ TranslatorsRepo::Format::Line };
		};

		class FindContext
		{
			friend class LinesRepo;

		public:
			enum class FindOptions { None, CaseSensitive };

		public:
			FindContext() = default;
			~FindContext() = default;

			bool isValid() const noexcept
			{
				return m_result.valid;
			}

			std::string_view query() const noexcept
			{
				return m_query;
			}

			std::tuple<size_t, size_t> position() const noexcept
			{
				if (!isValid())
					return { 0, 0 };

				return { m_result.lineIndex, m_result.lineOffset };
			}

		private:
			FindContext(std::string query, FindOptions options, bool isRegex)
				: m_isRegex{ isRegex }
				, m_query{ std::move(query) }
				, m_queryOptions{ options }
			{ }

			FindContext(std::string query, FindOptions options, bool isRegex, LogLine line, size_t lineIndex, size_t lineOffset)
				: FindContext{ std::move(query), options, isRegex }
			{
				m_result.valid = true;
				m_result.line = line;
				m_result.lineIndex = lineIndex;
				m_result.lineOffset = lineOffset;
			}

		private:
			bool m_isRegex{ false };
			std::string m_query;
			FindContext::FindOptions m_queryOptions;

			struct {
				bool valid{ false };
				LogLine line;
				size_t lineIndex{ 0 };
				size_t lineOffset{ 0 };
			} m_result;
		};

	public:
		static std::vector<std::string> listFolderFiles(FlavorsRepo::Type type, std::string_view folderPath);

		static std::unique_ptr<LinesRepo> initRepoFile(FlavorsRepo::Type type, std::string_view filePath);
		static std::unique_ptr<LinesRepo> initRepoFolder(FlavorsRepo::Type type, std::string_view folderPath);
		static std::unique_ptr<LinesRepo> initRepoFolder(FlavorsRepo::Type type, std::string_view folderPath, std::string_view fileNameFilterRegex);

		static std::unique_ptr<LinesRepo> initRepoFromCommnand(const LinesRepo& sourceRepo, std::string_view commandResult);

	public:
		~LinesRepo() = default;

		LinesRepo(const LinesRepo&) = delete;
		LinesRepo& operator=(const LinesRepo&) = delete;
		LinesRepo(LinesRepo&&) = delete;
		LinesRepo& operator=(LinesRepo&&) = delete;

		size_t numFiles() const noexcept;
		size_t numLines() const noexcept;
		FlavorsRepo::Type flavor() const noexcept;

		FindContext searchText(std::string_view query, FindContext::FindOptions options) const;
		FindContext searchTextRegex(std::string_view query, FindContext::FindOptions options) const;
		FindContext searchNext(FindContext ctx) const;

		std::string retrieveLineContent(size_t lineIndex, TranslatorsRepo::Type type, TranslatorsRepo::Format format) const;

		std::string getSummary() const;
		std::string getAvailableCommands() const;

		std::string executeCommand(std::string_view tag, std::string_view name) const;
		std::string executeCommand(std::string_view tag, std::string_view name, std::string_view params) const;

		bool exportLines(ExportOptions options, size_t indexStart, size_t count) const;
		bool exportCommandLines(ExportOptions options, std::string_view commandResult) const;
		bool exportCommandNetworkPackets(ExportOptions options, std::string_view commandResult) const;

	private:
		LinesRepo(std::shared_ptr<FilesRepo> repoFiles);
		LinesRepo(const LinesRepo& sourceRepo, std::vector<LogLine> logLines);

		void processData(const void* data, size_t dataSize);

	private:
		LinesTools m_linesTools;
		std::vector<LogLine> m_lines;

		std::unordered_map<std::string_view, std::vector<CommandsRepo::CommandInfo>> m_cmds;

		std::shared_ptr<FilesRepo> m_repoFiles;
	};
}

#endif
