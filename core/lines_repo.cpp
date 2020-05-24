#include "lines_repo.hpp"

#include "files_repo.hpp"

#include <set>
#include <regex>
#include <chrono>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <type_traits>

#include <nlohmann/json.hpp>

namespace la
{
	std::vector<std::string> LinesRepo::listFolderFiles(FlavorsRepo::Type type, std::string_view folderPath)
	{
		return FilesRepo::listFolderFiles(type, folderPath);
	}

	std::unique_ptr<LinesRepo> LinesRepo::initRepoFile(FlavorsRepo::Type type, std::string_view filePath)
	{
		auto repoFiles = FilesRepo::initRepoFile(type, filePath);
		if (!repoFiles)
			return nullptr;

		return std::unique_ptr<LinesRepo>{ new LinesRepo(std::move(repoFiles)) };
	}

	std::unique_ptr<LinesRepo> LinesRepo::initRepoFolder(FlavorsRepo::Type type, std::string_view folderPath)
	{
		auto repoFiles = FilesRepo::initRepoFolder(type, folderPath);
		if (!repoFiles)
			return nullptr;

		return std::unique_ptr<LinesRepo>{ new LinesRepo(std::move(repoFiles)) };
	}

	std::unique_ptr<LinesRepo> LinesRepo::initRepoFolder(FlavorsRepo::Type type, std::string_view folderPath, std::string_view fileNameFilterRegex)
	{
		auto repoFiles = FilesRepo::initRepoFolder(type, folderPath, fileNameFilterRegex);
		if (!repoFiles)
			return nullptr;

		return std::unique_ptr<LinesRepo>{ new LinesRepo(std::move(repoFiles)) };
	}

	LinesRepo::~LinesRepo() noexcept
	{ }

	size_t LinesRepo::numFiles() const noexcept
	{
		return m_repoFiles->numFiles();
	}

	size_t LinesRepo::numLines() const noexcept
	{
		return m_lines.size();
	}

	FlavorsRepo::Type LinesRepo::flavor() const noexcept
	{
		return m_repoFiles->flavor();
	}

	LinesRepo::FindContext LinesRepo::searchText(std::string_view query, FindContext::FindOptions options) const
	{
		if (query.empty())
			return LinesRepo::FindContext{ {}, options, false };

		std::boyer_moore_searcher bmSearcher(query.begin(), query.end());

		auto result = m_linesTools.windowSearch({ 0, m_lines.size() }, 0, [&bmSearcher](const char* dataStart, const char* dataEnd)
		{
			return std::search(dataStart, dataEnd, bmSearcher);
		});
		if (!result.valid)
			return LinesRepo::FindContext{ std::string{ query}, options, false };

		return LinesRepo::FindContext{ std::string{ query}, options, false, m_lines[result.lineIndex], result.lineIndex, result.lineOffset };
	}

	LinesRepo::FindContext LinesRepo::searchTextRegex(std::string_view query, FindContext::FindOptions options) const
	{
		if (query.empty())
			return LinesRepo::FindContext{ {}, options, true };

		std::regex regQuery;
		try
		{
			switch (options)
			{
			case FindContext::FindOptions::CaseSensitive:
				regQuery = std::regex{ std::string{ query }, std::regex::ECMAScript | std::regex::optimize };
				break;
			default:
				regQuery = std::regex{ std::string{ query }, std::regex::ECMAScript | std::regex::optimize | std::regex::icase };
				break;
			}
		}
		catch (std::regex_error exp)
		{ }

		auto result = m_linesTools.windowSearch({ 0, m_lines.size() }, 0, [&regQuery](const char* dataStart, const char* dataEnd)
		{
			std::cmatch matches;
			if (!std::regex_search(dataStart, dataEnd, matches, regQuery))
				return dataEnd;

			return matches[0].first;
		});
		if (!result.valid)
			return LinesRepo::FindContext{ std::string{ query}, options, true };

		return LinesRepo::FindContext{ std::string{ query}, options, true, m_lines[result.lineIndex], result.lineIndex, result.lineOffset };
	}

	LinesRepo::FindContext LinesRepo::searchNext(FindContext ctx) const
	{
		if (!ctx.m_result.valid)
			return ctx;

		if (ctx.m_isRegex)
		{
			std::regex regQuery;
			try
			{
				switch (ctx.m_queryOptions)
				{
				case FindContext::FindOptions::CaseSensitive:
					regQuery = std::regex{ ctx.m_query, std::regex::ECMAScript | std::regex::optimize };
					break;
				default:
					regQuery = std::regex{ ctx.m_query, std::regex::ECMAScript | std::regex::optimize | std::regex::icase };
					break;
				}
			}
			catch (std::regex_error exp)
			{ }

			auto result = m_linesTools.windowSearch({ ctx.m_result.lineIndex, m_lines.size() }, ctx.m_result.lineOffset + 1, [&regQuery](const char* dataStart, const char* dataEnd)
			{
				std::cmatch matches;
				if (!std::regex_search(dataStart, dataEnd, matches, regQuery))
					return dataEnd;

				return matches[0].first;
			});
			if (!result.valid)
				return LinesRepo::FindContext{ ctx.m_query, ctx.m_queryOptions, true };

			return LinesRepo::FindContext{ ctx.m_query, ctx.m_queryOptions, true, m_lines[result.lineIndex], result.lineIndex, result.lineOffset };
		}
		else
		{
			std::boyer_moore_searcher bmSearcher(ctx.m_query.begin(), ctx.m_query.end());

			auto result = m_linesTools.windowSearch({ ctx.m_result.lineIndex, m_lines.size() }, ctx.m_result.lineOffset + 1, [&bmSearcher](const char* dataStart, const char* dataEnd) { return std::search(dataStart, dataEnd, bmSearcher); });
			if (!result.valid)
				return LinesRepo::FindContext{ ctx.m_query, ctx.m_queryOptions, false };

			return LinesRepo::FindContext{ ctx.m_query, ctx.m_queryOptions, false, m_lines[result.lineIndex], result.lineIndex, result.lineOffset };
		}
	}

	std::string LinesRepo::retrieveLineContent(size_t lineIndex, TranslatorsRepo::Type type) const
	{
		if ((lineIndex < 0) || (lineIndex >= m_lines.size()))
			return {};

		const auto& line = m_lines[lineIndex];

		TranslatorsRepo::TranslationCtx translationCtx;
		return (TranslatorsRepo::translate(type, flavor(), line, translationCtx) ? translationCtx.output : "");
	}

	std::string LinesRepo::listAvailableCommands() const
	{
		auto jTags = nlohmann::json::array();
		for (const auto& [tag, cmds] : m_cmds)
		{
			auto jCmds = nlohmann::json::array();
			for (const auto& cmd : cmds)
			{
				nlohmann::json jCmd;
				jCmd["name"] = cmd.name;
				jCmd["help"] = cmd.help;
				if (!cmd.paramsHelp.empty())
					jCmd["paramsHelp"] = cmd.paramsHelp;
				jCmds.push_back(std::move(jCmd));
			}

			nlohmann::json jTag;
			jTag["name"] = tag;
			jTag["cmds"] = std::move(jCmds);

			jTags.push_back(std::move(jTag));
		}

		return jTags.dump();
	}

	std::string LinesRepo::executeCommand(std::string_view tag, std::string_view name) const
	{
		return executeCommand(tag, name, {});
	}

	std::string LinesRepo::executeCommand(std::string_view tag, std::string_view name, std::string_view params) const
	{
		nlohmann::json jResult;

		jResult["command"]["tag"] = tag;
		jResult["command"]["name"] = name;
		jResult["command"]["params"] = params;

		struct ResultCtx
			: public CommandsRepo::IResultCtx
		{
			ResultCtx(nlohmann::json& jsonLineIndices, nlohmann::json& jsonOutput)
				: m_jsonOutput{ jsonOutput }
				, m_jsonLineIndices{ jsonLineIndices }
			{ }

			nlohmann::json& json() noexcept override
			{
				return m_jsonOutput;
			}

			size_t addLineIndices(std::string_view name, const std::vector<size_t>& indices) override
			{
				size_t curIndex = m_jsonLineIndices.size();

				nlohmann::json jNewCol;
				if (!name.empty())
					jNewCol["name"] = name;
				jNewCol["indices"] = indices;

				m_jsonLineIndices.push_back(std::move(jNewCol));

				return curIndex;
			}

			nlohmann::json& m_jsonOutput;
			nlohmann::json& m_jsonLineIndices;
		};

		bool executed{ false };

		auto itCmds = m_cmds.find(tag);
		if (itCmds != m_cmds.end())
		{
			auto itCmd = std::find_if(itCmds->second.begin(), itCmds->second.end(), [&name](const auto& cmd) { return (cmd.name == name); });
			if (itCmd != itCmds->second.end())
			{
				executed = true;
				ResultCtx resultCtx{ jResult["linesIndices"], jResult["output"] };

				itCmd->executionCb(resultCtx, m_linesTools, params);
			}
		}

		if (jResult["output"].is_null())
			jResult.erase("output");

		jResult["executed"] = executed;
		return jResult.dump(1, '\t');
	}

	bool LinesRepo::exportLines(ExportOptions options, size_t indexStart, size_t count) const
	{
		if (options.filePath.empty())
			return false;

		if (m_lines.empty() || (indexStart >= m_lines.size()) || (count <= 0))
			return true;

		count = std::min(m_lines.size() - indexStart, count);

		auto path = std::filesystem::u8path(options.filePath);
		std::ofstream out(path.native().c_str(), std::ios::out | std::ios::binary | std::ios::ate | (options.appendToFile ? std::ios::app : std::ios::trunc));
		if (!out.good())
			return false;

		//as an optimization (in case of Raw translation type), we can export each file chunk all at once
		if (options.translationType == TranslatorsRepo::Type::Raw)
		{
			bool startGroupFound{ false };
			for (const auto& group : m_fileLineRanges)
			{
				if (!startGroupFound && ((indexStart < group.start) || (indexStart >= group.end)))
					continue;

				if (startGroupFound)
					indexStart = group.start;
				else
					startGroupFound = true;

				if (count <= (group.end - indexStart)) //if what is still left to write is in this entire group
				{
					auto bytesToWrite = static_cast<std::streamsize>(m_lines[indexStart + count - 1].data.end - m_lines[indexStart].data.start);
					out.write(m_lines[indexStart].data.start, bytesToWrite);
					out.write("\n", 1);

					count = 0;
					break;
				}

				//write until the end of the group and move on to the next

				auto bytesToWrite = static_cast<std::streamsize>(m_lines[group.end - 1].data.end - m_lines[indexStart].data.start);
				out.write(m_lines[indexStart].data.start, bytesToWrite);
				out.write("\n", 1);

				count -= (group.end - indexStart);
			}

			assert(count == 0);
			return true;
		}

		//reaching this point, we have to translate each line

		auto repoFlavor = flavor();
		TranslatorsRepo::TranslationCtx translationCtx;

		for (; count > 0; count--, indexStart++)
		{
			const auto& line = m_lines[indexStart];

			translationCtx.output.clear();
			translationCtx.auxiliary.clear();

			if (TranslatorsRepo::translate(options.translationType, repoFlavor, line, translationCtx))
				out.write(translationCtx.output.data(), translationCtx.output.size());
			else
				out.write(line.data.start, static_cast<size_t>(line.data.end - line.data.start));

			out.write("\n", 1);
		}

		return true;
	}

	bool LinesRepo::exportCommandResult(ExportOptions options, std::string_view commandResult) const
	{
		if (options.filePath.empty() || commandResult.empty())
			return false;

		auto jRoot = nlohmann::json::parse(commandResult);
		if (!jRoot.is_object() || !jRoot.contains("linesIndices") || !jRoot["linesIndices"].is_array())
			return false;

		auto path = std::filesystem::u8path(options.filePath);
		std::ofstream out(path.native().c_str(), std::ios::out | std::ios::binary | std::ios::ate | (options.appendToFile ? std::ios::app : std::ios::trunc));
		if (!out.good())
			return false;

		bool firstIndexGroup{ true };
		for (const auto& jIndexGroup : jRoot["linesIndices"])
		{
			if (!jIndexGroup.is_object() || !jIndexGroup.contains("indices"))
				continue;

			auto& jIndices = jIndexGroup["indices"];
			if (!jIndices.is_array() || jIndices.empty())
				continue;

			if (!std::exchange(firstIndexGroup, false))
				out.write("\n", 1);

			//as an optimization (in case of Raw translation type), we can export each line quicker
			if (options.translationType == TranslatorsRepo::Type::Raw)
			{
				for (const auto& index : jIndices)
				{
					auto value = index.get<size_t>();
					if ((value < 0) || (value >= m_lines.size()))
						continue;

					const auto& line = m_lines[value];
					out.write(line.data.start, static_cast<size_t>(line.data.end - line.data.start));
					out.write("\n", 1);
				}
			}
			else
			{
				//we have to translate each line

				auto repoFlavor = flavor();
				TranslatorsRepo::TranslationCtx translationCtx;

				for (const auto& index : jIndices)
				{
					auto value = index.get<size_t>();
					if ((value < 0) || (value >= m_lines.size()))
						continue;

					const auto& line = m_lines[value];

					translationCtx.output.clear();
					translationCtx.auxiliary.clear();

					if (TranslatorsRepo::translate(options.translationType, repoFlavor, line, translationCtx))
						out.write(translationCtx.output.data(), translationCtx.output.size());
					else
						out.write(line.data.start, static_cast<size_t>(line.data.end - line.data.start));

					out.write("\n", 1);
				}
			}

			out.flush();
		}

		return true;
	}

	LinesRepo::LinesRepo(std::unique_ptr<FilesRepo> repoFiles) noexcept
		: m_linesTools{ m_lines, m_fileLineRanges }
		, m_repoFiles{ std::move(repoFiles) }
	{
		m_repoFiles->iterateFiles([this](const void* data, size_t size)
		{
			processData(data, size);
		});

		CommandsRepo::iterateCommands(m_repoFiles->flavor(), [this](std::string_view tag, CommandsRepo::CommandInfo cmd)
		{
			auto& cmds = m_cmds[tag];

			if (std::find_if(cmds.begin(), cmds.end(), [&newCmd = cmd](const auto& cmd) { return (cmd.name == newCmd.name); }) != cmds.end())
				return;

			cmds.push_back(std::move(cmd));
		});
	}

	void LinesRepo::processData(const void* data, size_t dataSize)
	{
		size_t startIndex = m_lines.size();

		auto linesAdded = FlavorsRepo::processFileData(m_repoFiles->flavor(), data, dataSize, m_lines);
		if (linesAdded <= 0)
			return;

		m_fileLineRanges.push_back({ startIndex, startIndex + linesAdded });
	}
}
