#include "cmd_wcs_comlib.hpp"

#include "../lines_tools.hpp"
#include "cmd_wcs_comlib_utils.hpp"

#include <set>
#include <regex>
#include <optional>
#include <unordered_set>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace la
{
	namespace
	{
		template<class TCallback>
		std::vector<size_t> toolTasksExecutionsIf(const LinesTools& linesTools, TCallback&& cb)
		{
			std::vector<size_t> lineIndices;

			auto& lines = linesTools.lines();

			for (size_t lineIndex = 0; lineIndex < lines.size(); lineIndex++)
			{
				if (!cb(lines[lineIndex]))
					continue;

				auto taskId = CommandsCOMLibUtils::taskAtLine(linesTools, lineIndex);
				if (!taskId.has_value())
					continue;

				auto taskLineIndices = CommandsCOMLibUtils::taskFullExecution(linesTools, taskId.value(), { 0, lines.size() });
				if (taskLineIndices.empty())
					continue;

				assert(std::find(taskLineIndices.begin(), taskLineIndices.end(), lineIndex) != taskLineIndices.end());

				lineIndices.insert(lineIndices.end(), taskLineIndices.begin(), taskLineIndices.end());

				lineIndex = taskLineIndices.back(); //skip all the lines of this task
			}

			return lineIndices;
		}

		void cmdTaskExecution(CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, int64_t taskId, LinesTools::LineIndexRange lineRange)
		{
			auto lineIndices = CommandsCOMLibUtils::taskFullExecution(linesTools, taskId, lineRange);
			std::sort(lineIndices.begin(), lineIndices.end());

			resultCtx.addLineIndices(lineIndices);
		}

		void cmdTaskExecutions(CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view params, LinesTools::LineIndexRange lineRange)
		{
			auto& lines = linesTools.lines();
			if (lines.empty())
				return;

			//we support line execution
			if ((params.size() >= 2) && (params[0] == ':') && (params[1] != ':'))
			{
				size_t lineIndex;
				if (auto [p, ec] = std::from_chars(params.data() + 1, params.data() + params.size(), lineIndex); ec != std::errc())
					return;

				auto taskId = CommandsCOMLibUtils::taskAtLine(linesTools, lineIndex);
				if (taskId.has_value())
					cmdTaskExecution(resultCtx, linesTools, taskId.value(), lineRange);

				return;
			}

			//params can be the task id
			{
				int64_t taskId;
				if (auto [p, ec] = std::from_chars(params.data(), params.data() + params.size(), taskId); ec == std::errc())
				{
					cmdTaskExecution(resultCtx, linesTools, taskId, lineRange);
					return;
				}
			}

			//assume that params is the task name, which means we can have multiple executions

			std::vector< std::vector<size_t>> taskExecutions;
			while (lineRange.start < lineRange.end)
			{
				LinesTools::FilterCollection filter{
					LinesTools::FilterParam<LinesTools::FilterType::Tag, std::string_view>("COMLib.Scheduler"),
					LinesTools::FilterParam<LinesTools::FilterType::Msg, std::string_view>("task scheduled") };

				std::optional<int64_t> taskId;
				auto linesProcessed = linesTools.windowIterate({ lineRange.start, lineRange.end }, filter, [&taskId, &params](size_t, LogLine line, size_t lineIndex)
				{
					std::string_view taskName;
					if (!line.paramExtract("name", taskName) || (taskName != params))
						return true;

					//found it...
					int64_t id;
					if (!line.paramExtractAs<int64_t>("id", id))
						return true;

					taskId = id;
					return false;
				});

				if (taskId.has_value())
				{
					auto lineIndices = CommandsCOMLibUtils::taskFullExecution(linesTools, *taskId, { lineRange.start + linesProcessed - 1, lineRange.end });
					if (!lineIndices.empty())
					{
						std::sort(lineIndices.begin(), lineIndices.end());
						resultCtx.addLineIndices(lineIndices);
					}
				}

				lineRange.start += linesProcessed;
				assert(lineRange.start <= lineRange.end);
			}
		}

		void cmdDeadlocks(CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools)
		{
			struct ExecutionInfo
			{
				size_t lineIndexStart;
				size_t lineIndexEnd;
				std::vector<int64_t> threadIds;

				struct TaskInfo {
					std::string name;
					std::vector<size_t> lineIndices;
				};

				struct {
					std::set<int64_t> waiting;
					std::set<int64_t> finishing;
					std::set<int64_t> executing;
					std::unordered_map<int64_t, TaskInfo> info;
				} tasks;
			};
			std::vector<ExecutionInfo> executions;

			{
				enum class TaskStep { Unknown, Executing, Waiting, Finishing, Finished };

				//find all executions
				for (const auto& execRange : CommandsCOMLibUtils::executionsRanges(linesTools))
				{
					ExecutionInfo execution;
					execution.lineIndexStart = execRange.start;
					execution.lineIndexEnd = execRange.end;

					LinesTools::FilterCollection filter{
						LinesTools::FilterParam<LinesTools::FilterType::Tag, std::string_view>("COMLib.Scheduler") };

					[[maybe_unused]] auto linesProcessed = linesTools.windowIterate({ execRange.start, execRange.end }, filter, [&execution](size_t, LogLine line, size_t lineIndex)
					{
						TaskStep taskStep{ TaskStep::Unknown };
						{
							if (line.checkSectionMsg<LogLine::MatchType::StartsWith>("task waiting"))
								taskStep = TaskStep::Waiting;
							else if (line.checkSectionMsg<LogLine::MatchType::Exact>("task finishing"))
								taskStep = TaskStep::Finishing;
							else if (line.checkSectionMsg<LogLine::MatchType::Exact>("task finished"))
								taskStep = TaskStep::Finished;
							else if (line.checkSectionMsg<LogLine::MatchType::Exact>("task executing"))
								taskStep = TaskStep::Executing;

							if (taskStep == TaskStep::Unknown)
								return true;
						}

						if (taskStep == TaskStep::Executing) //gather thread ids
						{
							if (std::find(execution.threadIds.begin(), execution.threadIds.end(), line.threadId) == execution.threadIds.end())
								execution.threadIds.push_back(line.threadId);
						}

						int32_t taskId;
						if (!line.paramExtractAs<int32_t>("id", taskId))
							return true;

						switch (taskStep)
						{
						case TaskStep::Waiting:
							execution.tasks.waiting.insert(taskId);
							execution.tasks.finishing.erase(taskId);
							execution.tasks.executing.erase(taskId);
							break;
						case TaskStep::Finishing:
							execution.tasks.waiting.erase(taskId);
							execution.tasks.finishing.insert(taskId);
							execution.tasks.executing.erase(taskId);
							break;
						case TaskStep::Finished:
							execution.tasks.waiting.erase(taskId);
							execution.tasks.finishing.erase(taskId);
							execution.tasks.executing.erase(taskId);
							break;
						case TaskStep::Executing:
							execution.tasks.waiting.erase(taskId);
							execution.tasks.finishing.erase(taskId);
							execution.tasks.executing.insert(taskId);
							break;
						default:
							break;
						}

						return true;
					});

					assert(linesProcessed == execRange.numLines());

					//we have an execution

					{
						//gather task names and line indices

						for (auto id : execution.tasks.executing)
							execution.tasks.info.insert({ id, {} });
						for (auto id : execution.tasks.waiting)
							execution.tasks.info.insert({ id, {} });
						for (auto id : execution.tasks.finishing)
							execution.tasks.info.insert({ id, {} });

						auto& lines = linesTools.lines();

						for (auto& [taskId, taskInfo] : execution.tasks.info)
						{
							taskInfo.lineIndices = CommandsCOMLibUtils::taskFullExecution(linesTools, taskId, { execution.lineIndexStart, execution.lineIndexEnd });

							{
								auto result = linesTools.windowFindFirst({ execution.lineIndexStart, execution.lineIndexEnd }, fmt::format("| task scheduled | id={}; name=", taskId));
								if (!result.has_value())
									continue;

								const auto& line = lines[*result];
								if (!line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.Scheduler"))
									continue;

								line.paramExtractAs<std::string>("name", taskInfo.name);
							}
						}
					}

					executions.push_back(std::move(execution));
				}
			}

			//create a json with all the necessary information

			auto& jResult = resultCtx.json();

			for (const auto& exec : executions)
			{
				nlohmann::json jExec;

				jExec["lineIndexRange"] = { exec.lineIndexStart, exec.lineIndexEnd };
				jExec["threadIds"] = exec.threadIds;

				jExec["tasks"]["executing"] = exec.tasks.executing;
				jExec["tasks"]["waiting"] = exec.tasks.waiting;
				jExec["tasks"]["finishing"] = exec.tasks.finishing;

				jExec["tasks"]["data"] = nlohmann::json::array();

				auto& jTaskInfo = jExec["tasks"]["data"];
				for (const auto& [taskId, taskInfo] : exec.tasks.info)
				{
					nlohmann::json jInfo;
					jInfo["id"] = taskId;
					jInfo["name"] = taskInfo.name;
					jInfo["linesIndex"] = resultCtx.addLineIndices(taskInfo.lineIndices);
					jTaskInfo.push_back(std::move(jInfo));
				}

				jResult.push_back(std::move(jExec));
			}
		}

		void cmdMsgFlow(CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view params, LinesTools::LineIndexRange lineRange)
		{
			int32_t msgId{ 0 };
			std::string msgNetworkId;
			{
				std::optional<int32_t> paramId;
				{
					int32_t id;
					if (auto [p, ec] = std::from_chars(params.data(), params.data() + params.size(), id); ec == std::errc())
						paramId = id;
				}

				LinesTools::FilterCollection filter{
					LinesTools::FilterParam<LinesTools::FilterType::Tag, std::string_view>("COMLib.ChatController"),
					LinesTools::FilterParam<LinesTools::FilterType::Msg, std::string_view>("message stored") };

				linesTools.windowIterate({ lineRange.start, lineRange.end }, filter, [&msgId, &msgNetworkId, &params, &paramId](size_t, LogLine line, size_t lineIndex)
				{
					bool found{ false };
					if (paramId.has_value())
						found = line.paramCheck("id", *paramId);
					if (!found)
						found = line.paramCheck("networkId", params);
					if (!found)
						found = line.paramCheck("MessageNetworkId", params);

					if (!found)
						return true;

					if (!line.paramExtractAs<int32_t>("id", msgId))
						return true;
					if (!line.paramExtractAs<std::string>("networkId", msgNetworkId))
						return true;
					if (msgNetworkId.empty() && !line.paramExtractAs<std::string>("MessageNetworkId", msgNetworkId))
						return true;

					return false;
				});
			}

			if ((msgId <= 0) || msgNetworkId.empty())
				return;

			auto lineIndices = toolTasksExecutionsIf(linesTools, [&msgId, &msgNetworkId](const LogLine& line)
			{
				if (line.checkSectionTag<LogLine::MatchType::Exact>("COMLib.ChatController") && line.checkSectionMsg<LogLine::MatchType::Exact>("message stored"))
				{
					if (line.paramCheck("id", msgId))
						return true;
					if (line.paramCheck("networkId", msgNetworkId) || line.paramCheck("MessageNetworkId", msgNetworkId))
						return true;
				}

				return false;
			});

			resultCtx.addLineIndices(lineIndices);
		}

		void cmdSIPFlows(CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view params)
		{
			struct {
				std::regex extractCallID{ R"(Call-ID: (.*))", std::regex::ECMAScript | std::regex::icase };
				std::regex extractCSeq{ R"(CSeq: .+ (.+))", std::regex::ECMAScript | std::regex::icase };

				std::regex matchTX{ R"(\.TX \d+ bytes )", std::regex::ECMAScript | std::regex::icase };
				std::regex matchRX{ R"(\.RX \d+ bytes )", std::regex::ECMAScript | std::regex::icase };

				std::regex extractNetworkData{ R"(\) (\bto\b|\bfrom\b) (?:\bTCP\b|\bUDP\b) (\d+\.\d+\.\d+\.\d+:\d+):)" };
			} regexs;

			struct DialogData
			{
				std::string_view method;
				std::vector<size_t> txLineIndices;
				std::vector<size_t> rxLineIndices;
				std::vector<size_t> lineIndices;
			};

			std::unordered_map<std::string_view, DialogData> dialogs;

			const LinesTools::FilterCollection filter{
				LinesTools::FilterParam<LinesTools::FilterType::LogLevel, LogLevel>(LogLevel::Debug),
				LinesTools::FilterParam<LinesTools::FilterType::Tag, std::string_view>("COMLib.PJSIP"),
				LinesTools::FilterParam<LinesTools::FilterType::Msg, std::string_view, LogLine::MatchType::Contains>("pjsua_core.c") };

			//find all executions
			for (const auto& execRange : CommandsCOMLibUtils::executionsRanges(linesTools))
			{
				dialogs.clear();

				linesTools.windowIterate({ execRange.start, execRange.end }, filter, [&regexs, &dialogs, &resultCtx](size_t, LogLine line, size_t lineIndex)
				{
					auto content = line.getSectionMsg();

					//without a body, ignore this line
					std::string_view body;
					{
						auto offset = content.find(":\n");
						if ((offset == std::string_view::npos) || ((offset + 2) >= content.size()))
							return true;

						offset += 2;
						body = std::string_view{ line.data.start + line.sectionMsg.offset + offset, line.sectionMsg.size - offset };

						std::string_view ignoreSufix{ "\n--end msg--" };
						if ((body.size() >= ignoreSufix.size()) && (body.compare(body.length() - ignoreSufix.length(), ignoreSufix.length(), ignoreSufix) == 0))
							body = body.substr(0, body.size() - ignoreSufix.size());
					}

					//without a Call-ID, ignore this line
					std::string_view callId;
					{
						std::cmatch matches;
						if (!std::regex_search(content.data(), content.data() + content.size(), matches, regexs.extractCallID))
							return true;

						callId = std::string_view{ matches[1].first, static_cast<size_t>(matches[1].second - matches[1].first) };
					}

					//gather info into a dialog
					{
						auto& dialog = dialogs[callId];

						if (dialog.method.empty())
						{
							std::cmatch matches;
							if (std::regex_search(content.data(), content.data() + content.size(), matches, regexs.extractCSeq))
								dialog.method = { matches[1].first, static_cast<size_t>(matches[1].second - matches[1].first) };
						}

						if (std::regex_search(content.data(), content.data() + content.size(), regexs.matchTX))
							dialog.txLineIndices.push_back(lineIndex);
						else if (std::regex_search(content.data(), content.data() + content.size(), regexs.matchRX))
							dialog.rxLineIndices.push_back(lineIndex);

						dialog.lineIndices.push_back(lineIndex);
					}

					//gather network packet info
					{
						std::string srcAddress, dstAddress;

						std::cmatch matchesRoot;
						if (std::regex_search(content.data(), content.data() + content.size(), matchesRoot, regexs.extractNetworkData))
						{
							std::string_view dir{ matchesRoot[1].first, static_cast<size_t>(matchesRoot[1].second - matchesRoot[1].first) };
							assert((dir == "to") || (dir == "from"));

							std::string_view rootAddress{ matchesRoot[2].first, static_cast<size_t>(matchesRoot[2].second - matchesRoot[2].first) };

							CommandsRepo::IResultCtx::LineContent lineContent;
							lineContent.lineIndex = lineIndex;
							lineContent.contentOffset = body.data() - line.data.start;
							lineContent.contentSize = body.size();

							if (dir == "to")
								resultCtx.addNetworkPacketIPV4("127.0.0.1:0", rootAddress, line.timestamp, lineContent);
							else
								resultCtx.addNetworkPacketIPV4(rootAddress, "127.0.0.1:0", line.timestamp, lineContent);
						}
					}

					return true;
				});

				//create json with results for this execution
				{
					nlohmann::json jResult;
					jResult["lineIndexRange"] = { execRange.start, execRange.end };

					auto& jDialogs = jResult["dialogs"];
					for (const auto& [callId, diag] : dialogs)
					{
						if (!params.empty() && (diag.method != params))
							continue;

						assert((diag.rxLineIndices.size() + diag.txLineIndices.size()) == diag.lineIndices.size());

						nlohmann::json jInfo;
						jInfo["callId"] = callId;
						jInfo["method"] = diag.method;
						jInfo["txLineIndices"] = diag.txLineIndices;
						jInfo["rxLineIndices"] = diag.rxLineIndices;
						jInfo["linesIndex"] = resultCtx.addLineIndices(diag.lineIndices);
						jDialogs.push_back(std::move(jInfo));
					}

					resultCtx.json().push_back(std::move(jResult));
				}
			}
		}
	}

	CommandsRepo::Registry CommandsCOMLib::genCommandsRegistry()
	{
		CommandsRepo::Registry registry;

		registry.tag = "COMLib";
		registry.registerCommandsCb = [](CommandsRepo::IRegisterCtx& registerCtx)
		{
			auto flavor = registerCtx.flavor();
			if ((flavor != FlavorsRepo::Type::WCSCOMLib) && (flavor != FlavorsRepo::Type::WCSAndroidLogcat))
				return;

			registerCtx.registerCommand({ "Deadlocks", "Isolate every app execution and detect deadlocks in each one", {}, false,
				[](CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view) { return cmdDeadlocks(resultCtx, linesTools); } });

			registerCtx.registerCommand({ "Task execution", "Return all lines corresponding to a particular task execution", "task id or name", true,
				[](CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view cmdParams) { return cmdTaskExecutions(resultCtx, linesTools, cmdParams, { 0, linesTools.lines().size() }); } });

			registerCtx.registerCommand({ "Message flow", "Return all tasks that deal with a particular message", "msg id or networkId", false,
				[](CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view cmdParams) { return cmdMsgFlow(resultCtx, linesTools, cmdParams, { 0, linesTools.lines().size() }); } });

			registerCtx.registerCommand({ "SIP flows", "Return all log lines with SIP content", "optional SIP method name filter", false,
				[](CommandsRepo::IResultCtx& resultCtx, const LinesTools& linesTools, std::string_view cmdParams) { return cmdSIPFlows(resultCtx, linesTools, cmdParams); } });
		};

		return registry;
	}
}
