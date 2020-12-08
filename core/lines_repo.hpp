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

		struct FindOptions
		{
			enum class CaseSensitivity { None, CaseSensitive };

			FindOptions() = default;
			explicit FindOptions(CaseSensitivity caseSensitivity)
				: caseSensitivity{ caseSensitivity }
			{ }

			CaseSensitivity caseSensitivity{ CaseSensitivity::None };
			size_t startLine{ 0 }, startLineOffset{ 0 };
		};

		class FindContext
		{
			friend class LinesRepo;

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
			FindContext(std::string query, FindOptions::CaseSensitivity caseSensitivity, bool isRegex)
				: m_isRegex{ isRegex }
				, m_query{ std::move(query) }
				, m_caseSensitivity{ caseSensitivity }
			{ }

			FindContext(std::string query, FindOptions::CaseSensitivity caseSensitivity, bool isRegex, LogLine line, size_t lineIndex, size_t lineOffset)
				: FindContext{ std::move(query), caseSensitivity, isRegex }
			{
				m_result.valid = true;
				m_result.line = line;
				m_result.lineIndex = lineIndex;
				m_result.lineOffset = lineOffset;
			}

		private:
			bool m_isRegex{ false };
			std::string m_query;
			FindOptions::CaseSensitivity m_caseSensitivity;

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
		static std::unique_ptr<LinesRepo> initRepoFromLineRange(const LinesRepo& sourceRepo, size_t indexStart, size_t count);
		static std::unique_ptr<LinesRepo> initRepoFromTags(const LinesRepo& sourceRepo, const std::vector<std::string_view>& tags);

	public:
		~LinesRepo() = default;

		LinesRepo(const LinesRepo&) = delete;
		LinesRepo& operator=(const LinesRepo&) = delete;
		LinesRepo(LinesRepo&&) = delete;
		LinesRepo& operator=(LinesRepo&&) = delete;

		size_t numFiles() const noexcept;
		size_t numLines() const noexcept;
		FlavorsRepo::Type flavor() const noexcept;

		FindContext searchText(std::string_view query, FindOptions options) const;
		FindContext searchTextRegex(std::string_view query, FindOptions options) const;
		FindContext searchNext(FindContext ctx) const;

		std::string findAll(std::string_view query, FindOptions::CaseSensitivity caseSensitivity) const;
		std::string findAllRegex(std::string_view query, FindOptions::CaseSensitivity caseSensitivity) const;

		std::string retrieveLineContent(size_t lineIndex, TranslatorsRepo::Type type, TranslatorsRepo::Format format) const;

		std::optional<size_t> getLineIndex(int32_t lineId) const noexcept;
		std::string getSummary() const;
		std::string getAvailableCommands() const;

		std::string executeInspection() const;
		std::string executeCommand(std::string_view tag, std::string_view name) const;
		std::string executeCommand(std::string_view tag, std::string_view name, std::string_view params) const;

		bool exportLines(ExportOptions options, size_t indexStart, size_t count) const;
		bool exportCommandLines(ExportOptions options, std::string_view commandResult) const;
		bool exportCommandNetworkPackets(ExportOptions options, std::string_view commandResult) const;

	private:
		LinesRepo(std::shared_ptr<FilesRepo> repoFiles);
		LinesRepo(const LinesRepo& sourceRepo, std::vector<LogLine> logLines);

	private:
		LinesTools m_linesTools;
		std::vector<LogLine> m_lines;

		std::unordered_map<std::string_view, std::vector<CommandsRepo::CommandInfo>> m_cmds;

		std::shared_ptr<FilesRepo> m_repoFiles;
	};
}

#endif
