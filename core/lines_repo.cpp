#include "lines_repo.hpp"

#include "utils.hpp"
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

	std::unique_ptr<LinesRepo> LinesRepo::initRepoFromCommnand(const LinesRepo& sourceRepo, std::string_view commandResult)
	{
		if (commandResult.empty())
			return nullptr;

		std::vector<LogLine> logLines;
		{
			auto jRoot = nlohmann::json::parse(commandResult);
			if (!jRoot.is_object() || !jRoot.contains("linesIndices") || !jRoot["linesIndices"].is_array())
				return 0;

			bool firstIndexGroup{ true };
			for (const auto& jIndexGroup : jRoot["linesIndices"])
			{
				if (!jIndexGroup.is_object() || !jIndexGroup.contains("indices"))
					continue;

				auto& jIndices = jIndexGroup["indices"];
				if (!jIndices.is_array() || jIndices.empty())
					continue;

				for (const auto& index : jIndices)
				{
					auto value = index.get<size_t>();
					if ((value < 0) || (value >= sourceRepo.m_lines.size()))
						continue;

					logLines.push_back(sourceRepo.m_lines[value]);
				}
			}
		}

		if (logLines.empty())
			return nullptr;
		
		return std::unique_ptr<LinesRepo>{ new LinesRepo(sourceRepo, std::move(logLines)) };
	}

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

	std::string LinesRepo::retrieveLineContent(size_t lineIndex, TranslatorsRepo::Type type, TranslatorsRepo::Format format) const
	{
		if ((lineIndex < 0) || (lineIndex >= m_lines.size()))
			return {};

		const auto& line = m_lines[lineIndex];

		TranslatorsRepo::TranslationCtx translationCtx;
		return (TranslatorsRepo::translate(type, format, flavor(), line, translationCtx) ? translationCtx.output : "");
	}

	std::string LinesRepo::getSummary() const
	{
		auto jSummary = nlohmann::json::object();

		jSummary["timeRange"] = { m_lines.front().timestamp, m_lines.back().timestamp };
		jSummary["numLines"] = m_lines.size();

		{
			std::vector<size_t> lineIndices;
			{
				LinesTools::FilterCollection filter{
					LinesTools::FilterParam<LinesTools::FilterType::LogLevel, LogLevel>(LogLevel::Warn) };

				m_linesTools.windowIterate({ 0, m_lines.size() }, filter, [&lineIndices](size_t, LogLine, size_t lineIndex)
				{
					lineIndices.push_back(lineIndex);
					return true;
				});

				jSummary["warningsLinesIndex"] = lineIndices;
			}

			lineIndices.clear();
			{
				LinesTools::FilterCollection filter{
					LinesTools::FilterParam<LinesTools::FilterType::LogLevel, LogLevel>(LogLevel::Error) };

				m_linesTools.windowIterate({ 0, m_lines.size() }, filter, [&lineIndices](size_t, LogLine, size_t lineIndex)
				{
					lineIndices.push_back(lineIndex);
					return true;
				});

				jSummary["errorsLinesIndex"] = lineIndices;
			}
		}

		{
			std::set<int32_t> uniqueThreads;
			for (const auto& line : m_lines)
				uniqueThreads.insert(line.threadId);

			jSummary["threadIds"] = uniqueThreads;
		}

		{
			std::set<std::string_view> uniqueThreads;
			for (const auto& line : m_lines)
				uniqueThreads.insert(line.getSectionThreadName());

			jSummary["threadNames"] = uniqueThreads;
		}

		{
			std::set<std::string_view> uniqueTags;
			for (const auto& line : m_lines)
				uniqueTags.insert(line.getSectionTag());

			jSummary["tags"] = uniqueTags;
		}

		return jSummary.dump();
	}

	std::string LinesRepo::getAvailableCommands() const
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

		class ResultCtx
			: public CommandsRepo::IResultCtx
		{
		public:
			ResultCtx(nlohmann::json& jsonLineIndices, nlohmann::json& jsonNetworkPackets, nlohmann::json& jsonOutput)
				: m_jsonOutput{ jsonOutput }
				, m_jsonLineIndices{ jsonLineIndices }
				, m_jsonNetworkPackets{ jsonNetworkPackets }
			{ }

			nlohmann::json& json() noexcept override
			{
				return m_jsonOutput;
			}

			void addNetworkPacketIPV4(std::string_view srcAddress, std::string_view dstAddress, int64_t timestamp, LineContent lineContent) override
			{
				if (srcAddress.empty() || dstAddress.empty())
					return;

				addNetworkPacket("ipv4", srcAddress, dstAddress, timestamp, lineContent);
			}

			void addNetworkPacketIPV6(std::string_view srcAddress, std::string_view dstAddress, int64_t timestamp, LineContent lineContent) override
			{
				if (srcAddress.empty() || dstAddress.empty())
					return;

				addNetworkPacket("ipv6", srcAddress, dstAddress, timestamp, lineContent);
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

		private:
			void addNetworkPacket(std::string_view domain, std::string_view srcAddress, std::string_view dstAddress, int64_t timestamp, LineContent lineContent)
			{
				nlohmann::json jNewPacket;
				jNewPacket["domain"] = domain;
				jNewPacket["timestamp"] = timestamp;

				auto& jEndpoints = jNewPacket["endpoints"];
				jEndpoints.push_back(srcAddress);
				jEndpoints.push_back(dstAddress);

				auto& jLine = jNewPacket["line"];
				jLine["index"] = lineContent.lineIndex;
				jLine["offset"] = lineContent.contentOffset;
				jLine["size"] = lineContent.contentSize;

				m_jsonNetworkPackets.push_back(std::move(jNewPacket));
			}

		private:
			nlohmann::json& m_jsonOutput;
			nlohmann::json& m_jsonLineIndices;
			nlohmann::json& m_jsonNetworkPackets;
		};

		bool executed{ false };

		auto itCmds = m_cmds.find(tag);
		if (itCmds != m_cmds.end())
		{
			auto itCmd = std::find_if(itCmds->second.begin(), itCmds->second.end(), [&name](const auto& cmd) { return (cmd.name == name); });
			if (itCmd != itCmds->second.end())
			{
				executed = true;
				ResultCtx resultCtx{ jResult["linesIndices"], jResult["networkPackets"], jResult["output"] };

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
		std::ofstream out(path.native().c_str(), std::ios::out | std::ios::binary | std::ios::ate | (options.appendToFile ? 0 : std::ios::trunc));
		if (!out.good())
			return false;

		//as an optimization (in case of Raw translation type)
		if (options.translationType == TranslatorsRepo::Type::Raw)
		{
			for (; count > 0; count--, indexStart++)
			{
				const auto& line = m_lines[indexStart];

				out.write(line.data.start, static_cast<size_t>(line.data.end - line.data.start));
				out.put('\n');
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

			if (TranslatorsRepo::translate(options.translationType, options.translationFormat, repoFlavor, line, translationCtx))
				out.write(translationCtx.output.data(), translationCtx.output.size());
			else
				out.write(line.data.start, static_cast<size_t>(line.data.end - line.data.start));

			out.write("\n", 1);
		}

		assert(count == 0);
		return true;
	}

	bool LinesRepo::exportCommandLines(ExportOptions options, std::string_view commandResult) const
	{
		if (options.filePath.empty() || commandResult.empty())
			return false;

		auto jRoot = nlohmann::json::parse(commandResult);
		if (!jRoot.is_object() || !jRoot.contains("linesIndices") || !jRoot["linesIndices"].is_array())
			return false;

		auto path = std::filesystem::u8path(options.filePath);
		std::ofstream out(path.native().c_str(), std::ios::out | std::ios::binary | std::ios::ate | (options.appendToFile ? 0 : std::ios::trunc));
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

					if (TranslatorsRepo::translate(options.translationType, options.translationFormat, repoFlavor, line, translationCtx))
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

	bool LinesRepo::exportCommandNetworkPackets(ExportOptions options, std::string_view commandResult) const
	{
		if (options.filePath.empty() || commandResult.empty())
			return false;

		auto jRoot = nlohmann::json::parse(commandResult);
		if (!jRoot.is_object() || !jRoot.contains("networkPackets") || !jRoot["networkPackets"].is_array())
			return false;

		auto path = std::filesystem::u8path(options.filePath);
		std::ofstream out(path.native().c_str(), std::ios::out | std::ios::binary | std::ios::ate | (options.appendToFile ? 0 : std::ios::trunc));
		if (!out.good())
			return false;

		utils::Network::writePCAPHeader(out);

		for (const auto& jPacket : jRoot["networkPackets"])
		{
			if (!jPacket.contains("domain") || !jPacket.contains("timestamp") || !jPacket.contains("endpoints") || !jPacket.contains("line"))
				continue;

			auto vDomain = jPacket["domain"].get<std::string_view>();
			if ((vDomain != "ipv4") && (vDomain != "ipv6"))
				continue;

			std::string_view vSrcAddress, vDstAddress;
			{
				auto& jEndpoints = jPacket["endpoints"];
				if (!jEndpoints.is_array() || (jEndpoints.size() != 2))
					continue;

				vSrcAddress = jEndpoints[0].get<std::string_view>();
				vDstAddress = jEndpoints[1].get<std::string_view>();
				if (vSrcAddress.empty() || vDstAddress.empty())
					continue;
			}

			auto vTimestamp = jPacket["timestamp"].get<int64_t>();

			std::string_view vPayload;
			{
				auto& jLine = jPacket["line"];
				if (!jLine.contains("index") || !jLine.contains("offset") || !jLine.contains("size"))
					continue;

				auto lineIndex = jLine["index"].get<size_t>();
				if ((lineIndex < 0) || (lineIndex >= m_lines.size()))
					continue;

				auto lineContent = m_lines[lineIndex].toStr();

				auto contentOffset = jLine["offset"].get<size_t>();
				auto contentSize = jLine["size"].get<size_t>();
				if ((contentOffset >= lineContent.size()) || ((contentOffset + contentSize) > lineContent.size()))
					continue;

				vPayload = lineContent.substr(contentOffset, contentSize);
			}

			if (vDomain == "ipv4")
				utils::Network::writePCAPDataIPV4(out, vSrcAddress, vDstAddress, vTimestamp, { vPayload.data(), vPayload.size() });
			else
				utils::Network::writePCAPDataIPV6(out, vSrcAddress, vDstAddress, vTimestamp, { vPayload.data(), vPayload.size() });
		}

		return true;
	}

	LinesRepo::LinesRepo(std::shared_ptr<FilesRepo> repoFiles)
		: m_linesTools{ m_lines }
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

	LinesRepo::LinesRepo(const LinesRepo& sourceRepo, std::vector<LogLine> logLines)
		: m_linesTools{ m_lines }
		, m_lines{ std::move(logLines) }
		, m_cmds{ sourceRepo.m_cmds } //can reuse all the same commands
		, m_repoFiles{ sourceRepo.m_repoFiles } //store a reference to the files
	{

	}

	void LinesRepo::processData(const void* data, size_t dataSize)
	{
		FlavorsRepo::processFileData(m_repoFiles->flavor(), data, dataSize, m_lines);
	}
}
