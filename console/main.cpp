#include <lines_repo.hpp>

#include <chrono>
#include <memory>
#include <random>
#include <charconv>
#include <iostream>
#include <filesystem>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <cxxopts/cxxopts.hpp>

#if defined(_WIN32) || defined(WIN32)
	#include <windows.h>
#endif

namespace
{
	struct Context
	{
		std::string tag;

		struct
		{
			std::string name;
			std::string result;
		} cmd;

		la::LinesRepo::FindContext search;

		std::vector<std::unique_ptr<la::LinesRepo>> repoStack;
	};

	std::vector<std::string> parseParams(std::string_view params)
	{
		std::vector<std::string> res;

		auto walker = params.data();
		auto walkerEnd = walker + params.size();

		while (walker < walkerEnd)
		{
			while ((walker < walkerEnd) && (walker[0] == ' ')) //trim
				walker++;
			if (walker >= walkerEnd)
				continue;

			auto next = walker + 1;

			if (walker[0] == '"')
			{
				walker++;
				while ((next < walkerEnd) && (next[0] != '"'))
					next++;
			}
			else
			{
				while ((next < walkerEnd) && (next[0] != ' '))
					next++;
			}

			if (next > walker)
				res.push_back(std::string{ walker, static_cast<size_t>(next - walker) });

			if (next >= walkerEnd)
				break;

			params.remove_prefix(next - params.data() + 1);

			walker = params.data();
			walkerEnd = walker + params.size();
		}

		return res;
	}

	std::string convertToUTF8(std::string str)
	{
		if (str.empty())
			return str;

#if defined(_WIN32) || defined(WIN32)

		std::unique_ptr<wchar_t[]> wPath;
		{
			auto result = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);
			if (result <= 0)
				return {};

			wPath = std::unique_ptr<wchar_t[]>{ new wchar_t[result] };
			if (MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, wPath.get(), result) != result)
				return {};
		}

		std::unique_ptr<char> utf8Path;
		{
			auto result = WideCharToMultiByte(CP_UTF8, 0, wPath.get(), -1, nullptr, 0, nullptr, nullptr);
			if (result <= 0)
				return {};

			utf8Path = std::unique_ptr<char>{ new char[result] };
			if (WideCharToMultiByte(CP_UTF8, 0, wPath.get(), -1, utf8Path.get(), result, nullptr, nullptr) != result)
				return {};
		}

		return std::string{ utf8Path.get() };
#else
		return str;
#endif
	}

	bool initSystem()
	{
		//for Windows, set console input and output to UTF8 and enable ANSI escape codes
#if defined(_WIN32) || defined(WIN32)

		if (!SetConsoleCP(CP_UTF8) || !SetConsoleOutputCP(CP_UTF8))
			return false;

		auto cHandle = GetStdHandle(STD_INPUT_HANDLE);
		if (cHandle != INVALID_HANDLE_VALUE)
		{
			DWORD cMode;
			if (GetConsoleMode(cHandle, &cMode) && !(cMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
			{
				cMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
				if (!SetConsoleMode(cHandle, cMode))
					return false;
			}
		}

		cHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (cHandle != INVALID_HANDLE_VALUE)
		{
			DWORD cMode;
			if (GetConsoleMode(cHandle, &cMode) && !(cMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
			{
				cMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
				if (!SetConsoleMode(cHandle, cMode))
					return false;
			}
		}

		return true;
#else
		return true;
#endif
	}

	std::unique_ptr<la::LinesRepo> initRepo(int argc, char* argv[])
	{
		cxxopts::Options options("LogAnalyzer", "Analyze WCL (comlib, UIs and server) logs");

		options
			.positional_help("<path to folder or file>")
			.show_positional_help();

		options.add_options()
			("h,help", R"(Show this help)", cxxopts::value<bool>()->default_value("false"))
			("t,type", R"(Type of logs to process)", cxxopts::value<std::string>(), R"("comlib", "server" or "androidLogcat")")
			("f,file", R"(Parameter "path" is a file instead of a folder)", cxxopts::value<bool>()->default_value("false"))
			("F,fileFilter", R"-(Regex to filter which files are read from the target folder (ignored if "-f" option is used))-", cxxopts::value<std::string>());

		options.add_options("POSITIONAL")
			("path", R"(Folder or file path (if "-f" is specified) to process)", cxxopts::value<std::vector<std::string>>());

		options.parse_positional("path");

		auto result = options.parse(argc, argv);
		if (result.count("h"))
		{
			std::cout << options.help({ "" }) << std::endl;
			return nullptr;
		}

		if (result.count("t") != 1)
		{
			std::cerr << R"(Must have exactly one argument of "t")" << std::endl;
			return nullptr;
		}

		if (result.count("F") > 1)
		{
			std::cerr << R"-(Cannot have more than one file filter ("F") argument)-" << std::endl;
			return nullptr;
		}

		if (result.count("path") != 1)
		{
			std::cerr << R"(Only one path can be specified)" << std::endl;
			return nullptr;
		}

		auto oIsFile = result["f"].as<bool>();
		auto oFileType = convertToUTF8(result["t"].as<std::string>());
		auto oFileFilter = convertToUTF8((result.count("F") == 1) ? result["F"].as<std::string>() : std::string{});
		auto oPath = convertToUTF8(result["path"].as<std::vector<std::string>>().front());

		la::FlavorsRepo::Type flavorType;
		if (oFileType == "comlib")
			flavorType = la::FlavorsRepo::Type::WCSCOMLib;
		else if (oFileType == "server")
			flavorType = la::FlavorsRepo::Type::WCSServer;
		else if (oFileType == "androidLogcat")
			flavorType = la::FlavorsRepo::Type::WCSAndroidLogcat;
		else
		{
			std::cerr << "Unknown file type: " << oFileType << std::endl;
			return nullptr;
		}

		//init the repo

		std::unique_ptr<la::LinesRepo> repoLines;

		auto timestamp = std::chrono::high_resolution_clock::now();

		if (!oIsFile)
		{
			if (oFileFilter.empty())
				repoLines = la::LinesRepo::initRepoFolder(flavorType, oPath);
			else
				repoLines = la::LinesRepo::initRepoFolder(flavorType, oPath, oFileFilter);
		}
		else
		{
			repoLines = la::LinesRepo::initRepoFile(flavorType, oPath);
		}

		if (repoLines->numLines() <= 0)
		{
			std::cout << (oIsFile ? "No valid lines found inside file: " : "No valid lines found inside files in folder: ") << oPath << std::endl;
			return nullptr;
		}

		auto delta = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(std::chrono::high_resolution_clock::now() - timestamp).count();
		std::cout << fmt::format("Time to parse {0} files (with a total of {1} lines): {2:.2f} ms", repoLines->numFiles(), repoLines->numLines(), delta) << std::endl;

		return repoLines;
	}
}

int main(int argc, char* argv[])
{
	if (!initSystem())
		return 1;

	auto repoLines = initRepo(argc, argv);
	if (!repoLines)
		return 0;

	Context ctx;
	{
		//we can set the tag automatically if only one tag is available
		auto cmds = repoLines->listAvailableCommands();
		if (!cmds.empty())
		{
			auto jTags = nlohmann::json::parse(cmds);
			if (jTags.is_array() && (jTags.size() == 1))
				ctx.tag = jTags[0]["name"].get<std::string>();
		}
	}

	std::string args;
	while (true)
	{
		//prompt
		{
			auto pStack = fmt::format("\x1B[34m[{:*>{}}]", "", ctx.repoStack.size());
			auto pSearch = ctx.search.isValid() ? fmt::format("\x1B[92msearch: \x1B[32m\"{}\" ", ctx.search.query()) : "";
			auto pCmd = !ctx.cmd.result.empty() ? fmt::format("\x1B[92mcmd: \x1B[32m\"{}\" ", ctx.cmd.name) : "";
			
			std::cout << fmt::format("{} \x1B[92mtag: \x1B[32m\"{}\" {}{}\x1B[31m\\>\x1B[0m ", pStack, ctx.tag, pSearch, pCmd);
		}

		args.clear();
		std::getline(std::cin, args);

		auto params = parseParams(args);

		if ((params.size() == 1) && ((params[0] == "q") || (params[0] == "quit")))
			break;

		if ((params.size() == 1) && ((params[0] == "l") || (params[0] == "list")))
		{
			auto cmds = repoLines->listAvailableCommands();
			if (cmds.empty())
			{
				std::cout << "there are no commands available" << std::endl;
				continue;
			}

			auto jTags = nlohmann::json::parse(cmds);
			for (const auto& jTag : jTags)
			{
				std::cout << fmt::format("Available commands for \"{}\" tag:", jTag["name"].get<std::string_view>()) << std::endl;

				for (const auto& jCmd : jTag["cmds"])
				{
					std::cout << "\t" << fmt::format("Name: \"{}\"", jCmd["name"].get<std::string_view>()) << std::endl;
					std::cout << "\t" << fmt::format("Help: \"{}\"", jCmd["help"].get<std::string_view>()) << std::endl;
					if (jCmd.count("paramsHelp") == 1)
						std::cout << "\t" << fmt::format("Parameters: \"{}\"", jCmd["paramsHelp"].get<std::string_view>()) << std::endl;
					std::cout << std::endl;
				}
			}

			continue;
		}

		if ((params.size() >= 2) && ((params[0] == "s") || (params[0] == "set")))
		{
			if ((params[1] == "tag") && (params.size() == 3))
				ctx.tag = params[2];

			continue;
		}

		if ((params.size() >= 2) && ((params[0] == "e") || (params[0] == "exec")))
		{
			if (ctx.tag.empty())
			{
				std::cout << "must have a valid tag set to execute a command" << std::endl;
				continue;
			}

			auto timestamp = std::chrono::high_resolution_clock::now();

			auto result = repoLines->executeCommand(ctx.tag, params[1], (params.size() >= 3) ? params[2] : std::string{});
			if (result.empty())
				continue;

			auto delta = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(std::chrono::high_resolution_clock::now() - timestamp).count();

			{
				auto jRoot = nlohmann::json::parse(result);
				if (!jRoot.is_object() || !jRoot.contains("executed") || !jRoot["executed"].get<bool>())
				{
					std::cout << "error executing command" << std::endl;
					continue;
				}

				ctx.cmd.name = params[1];
				ctx.cmd.result = result;
			}

			std::cout << fmt::format("command executed successfully in {:.2f} ms)", delta) << std::endl;
			continue;
		}

		if ((params.size() >= 1) && ((params[0] == "p") || (params[0] == "print")))
		{
			//print all content of all the lines
			if (params.size() == 1)
			{
				if (ctx.cmd.result.empty())
				{
					std::cout << "no command to print" << std::endl;
					continue;
				}

				auto jRoot = nlohmann::json::parse(ctx.cmd.result);
				if (!jRoot.is_object() || !jRoot.contains("linesIndices") || !jRoot["linesIndices"].is_array())
				{
					std::cout << "current command doesn't contain any line indices" << std::endl;
					continue;
				}

				bool firstIndexGroup{ true };
				for (const auto& jIndexGroup : jRoot["linesIndices"])
				{
					if (!jIndexGroup.is_object() || !jIndexGroup.contains("indices"))
						continue;

					auto& jIndices = jIndexGroup["indices"];
					if (!jIndices.is_array() || jIndices.empty())
						continue;

					if (!std::exchange(firstIndexGroup, false))
						std::cout << std::endl;

					for (const auto& index : jIndices)
						std::cout << repoLines->retrieveLineContent(index.get<size_t>(), la::TranslatorsRepo::Type::Translated) << std::endl;
				}

				continue;
			}

			//print a specific line
			if (params.size() == 2)
			{
				if (params[1] == "-json")
				{
					std::cout << ctx.cmd.result << std::endl;
					continue;
				}

				//try as a line index
				{
					size_t lineIndex;
					if (auto [p, ec] = std::from_chars(params[1].data(), params[1].data() + params[1].size(), lineIndex); ec == std::errc())
					{
						std::cout << repoLines->retrieveLineContent(lineIndex, la::TranslatorsRepo::Type::Translated) << std::endl;
						continue;
					}
				}

				continue;
			}

			continue;
		}

		if ((params.size() == 1) && ((params[0] == "push") || (params[0] == "pop")))
		{
			if (params[0] == "push")
			{
				if (ctx.cmd.result.empty())
				{
					std::cout << "no command results available" << std::endl;
					continue;
				}

				auto newRepo = la::LinesRepo::initRepoFromCommnand(*repoLines.get(), ctx.cmd.result);
				if (!newRepo)
				{
					std::cerr << "unable to create new repo from command results" << std::endl;
					continue;
				}

				ctx.repoStack.push_back(std::move(repoLines));
				repoLines = std::exchange(newRepo, nullptr);

				ctx.cmd.name.clear();
				ctx.cmd.result.clear();
				ctx.search = la::LinesRepo::FindContext{};

				std::cout << fmt::format("new repo activated (with a total of {0} lines)", repoLines->numLines()) << std::endl;
			}
			else
			{
				if (ctx.repoStack.empty())
				{
					std::cout << "there's no repo in stack" << std::endl;
					continue;
				}

				repoLines = std::exchange(ctx.repoStack.back(), nullptr);
				ctx.repoStack.pop_back();
			}

			continue;
		}

		if ((params.size() >= 2) && (params.size() <= 3) && ((params[0] == "ex") || (params[0] == "export")))
		{
			if (ctx.cmd.result.empty())
			{
				std::cout << "no command to export" << std::endl;
				continue;
			}

			if (params.size() == 2)
			{
				la::LinesRepo::ExportOptions options;
				options.filePath = params[1];
				options.appendToFile = true;
				options.translationType = la::TranslatorsRepo::Type::Translated;

				if (!repoLines->exportCommandLines(options, ctx.cmd.result))
					std::cout << "error exporting content to file" << std::endl;
				continue;
			}

			if ((params.size() == 3) && (params[1] == "-pcap"))
			{
				la::LinesRepo::ExportOptions options;
				options.filePath = params[2];
				options.appendToFile = false;
				options.translationType = la::TranslatorsRepo::Type::Raw;

				if (!repoLines->exportCommandNetworkPackets(options, ctx.cmd.result))
					std::cout << "error exporting content to file" << std::endl;
				continue;
			}

			continue;
		}

		if ((params.size() == 2) && (params[0] == "exportAll"))
		{
			auto timestamp = std::chrono::high_resolution_clock::now();

			la::LinesRepo::ExportOptions options;
			options.filePath = params[1];
			options.appendToFile = false;
			options.translationType = la::TranslatorsRepo::Type::Translated;

			if (!repoLines->exportLines(options, 0, repoLines->numLines()))
				std::cout << "error exporting all lines to file" << std::endl;

			auto delta = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(std::chrono::high_resolution_clock::now() - timestamp).count();
			std::cout << fmt::format("(exported all log lines in {:.2f} ms)", delta) << std::endl;

			continue;
		}

		if ((params.size() >= 1) && (params.size() <= 3) && ((params[0] == "f") || (params[0] == "find")))
		{
			if (params.size() == 1)
			{
				if (!ctx.search.isValid())
					std::cout << "must have a valid search to proceed to the next result" << std::endl;

				ctx.search = repoLines->searchNext(ctx.search);
				if (!ctx.search.isValid())
				{
					std::cout << "no more instances found" << std::endl;
					continue;
				}

				std::cout << repoLines->retrieveLineContent(std::get<0>(ctx.search.position()), la::TranslatorsRepo::Type::Translated) << std::endl;
				std::cout << fmt::format("{:>{}}", "^", std::get<1>(ctx.search.position()) + 1) << std::endl;
			}
			else if (params.size() >= 2)
			{
				if ((params.size() == 3) && (params[1] == "-regex"))
					ctx.search = repoLines->searchTextRegex(params[2], la::LinesRepo::FindContext::FindOptions::None);
				else
					ctx.search = repoLines->searchText(params[1], la::LinesRepo::FindContext::FindOptions::None);

				if (!ctx.search.isValid())
				{
					std::cout << "can't find instances of \"" << params[1] << "\"" << std::endl;
					continue;
				}

				std::cout << repoLines->retrieveLineContent(std::get<0>(ctx.search.position()), la::TranslatorsRepo::Type::Translated) << std::endl;
				std::cout << fmt::format("{:>{}}", "^", std::get<1>(ctx.search.position()) + 1) << std::endl;
			}

			continue;
		}

		if ((params.size() >= 2) && (params.size() <= 3) && (params[0] == "findAll"))
		{
			auto timestamp = std::chrono::high_resolution_clock::now();

			la::LinesRepo::FindContext result;

			if ((params.size() == 3) && (params[1] == "-regex"))
				result = repoLines->searchTextRegex(params[2], la::LinesRepo::FindContext::FindOptions::None);
			else
				result = repoLines->searchText(params[1], la::LinesRepo::FindContext::FindOptions::None);

			size_t count{ 0 };
			while (result.isValid())
			{
				count++;
				std::cout << repoLines->retrieveLineContent(std::get<0>(result.position()), la::TranslatorsRepo::Type::Translated) << std::endl;
				result = repoLines->searchNext(result);
			}

			auto delta = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(std::chrono::high_resolution_clock::now() - timestamp).count();
			std::cout << fmt::format("(found {} results in {:.2f} ms)", count, delta) << std::endl;

			continue;
		}

		if ((params.size() == 1) && ((params[0] == "h") || (params[0] == "help")))
		{
			std::cout << "available keywords: " << std::endl;
			std::cout << "\t l[ist] - list available commands" << std::endl;
			std::cout << "\t e[xec] - execute a command" << std::endl;
			std::cout << "\t p[rint] - print stuff" << std::endl;
			std::cout << "\t push/pop - push or pop repo using current command result" << std::endl;
			std::cout << "\t ex[port] - export data to a file" << std::endl;
			std::cout << "\t exportAll - export data to a file" << std::endl;
			std::cout << "\t f[ind] - find a specific text" << std::endl;
			std::cout << "\t findAll - find all instances of a specific text" << std::endl;
			std::cout << "\t s[et] - update a env variable:" << std::endl;
			std::cout << "\t\t tag - update the tag used to exec commands" << std::endl;
			std::cout << "\t q[uit] - quit" << std::endl;
			std::cout << "\t h[elp] - show this help" << std::endl;
			continue;
		}
	}

	return 0;
}
