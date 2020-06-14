#include "api.h"

#include <lines_repo.hpp>

namespace
{
	laStrUTF8 convertStr(std::string_view str)
	{
		if (str.empty())
			return la_str_init();

		auto newStr = la_str_init_length(static_cast<int>(str.size()));
		std::memcpy(newStr.data, str.data(), str.size());

		return newStr;
	}

	la::LinesRepo::FindContext::FindOptions convertSearchOptions(const laSearchOptions options)
	{
		switch (options)
		{
		case LA_SEARCH_OPTION_CASE_SENSITIVE:
			return la::LinesRepo::FindContext::FindOptions::CaseSensitive;
		case LA_SEARCH_OPTION_NONE:
		default:
			return la::LinesRepo::FindContext::FindOptions::None;
		}
	}

	la::LinesRepo::ExportOptions convertExportOptions(const laExportOptions* options)
	{
		if (!options)
			return {};

		la::LinesRepo::ExportOptions nOptions;
		nOptions.filePath = { options->filePath.data, static_cast<size_t>(options->filePath.size) };
		nOptions.appendToFile = (options->appendToFile != 0);

		switch (options->translationType)
		{
		case LA_TRANSLATOR_TYPE_RAW_JSON:
			nOptions.translationType = la::TranslatorsRepo::Type::RawJSON;
			break;
		case LA_TRANSLATOR_TYPE_TRANSLATED:
			nOptions.translationType = la::TranslatorsRepo::Type::Translated;
			break;
		case LA_TRANSLATOR_TYPE_TRANSLATED_JSON:
			nOptions.translationType = la::TranslatorsRepo::Type::TranslatedJSON;
			break;
		case LA_TRANSLATOR_TYPE_RAW:
		default:
			nOptions.translationType = la::TranslatorsRepo::Type::Raw;
			break;
		}

		return nOptions;
	}
}

laStrUTF8 la_str_init()
{
	laStrUTF8 str;
	str.data = nullptr;
	str.size = 0;

	return str;
}

laStrUTF8 la_str_init_copy(const char* cstr)
{
	if (!cstr || (*cstr == '\0'))
		return la_str_init();

	auto str = la_str_init_length(static_cast<int>(std::strlen(cstr)));
	std::memcpy(str.data, cstr, str.size);

	return str;
}

laStrUTF8 la_str_init_length(int size)
{
	if (size <= 0)
		return la_str_init();

	laStrUTF8 str;
	str.size = size;
	str.data = reinterpret_cast<char*>(std::malloc(str.size));

	return (str.data ? str : la_str_init());
}

void la_str_destroy(laStrUTF8* str)
{
	if (!str)
		return;

	if (str->data)
		std::free(str->data);

	str->data = nullptr;
	str->size = 0;
}

laStrFixedUTF8 la_str_fixed_init()
{
	laStrFixedUTF8 str;
	str.data = nullptr;
	str.size = 0;

	return str;
}

laStrFixedUTF8 la_str_fixed_init_str(laStrUTF8* str)
{
	if (!str)
		return la_str_fixed_init();

	laStrFixedUTF8 strFixed;
	strFixed.data = str->data;
	strFixed.size = str->size;

	return strFixed;
}

laStrFixedUTF8 la_str_fixed_init_cstr(const char* cstr)
{
	if (!cstr || (*cstr == '\0'))
		return la_str_fixed_init();

	laStrFixedUTF8 str;
	str.data = cstr;
	str.size = std::strlen(cstr);

	return str;
}

int la_find_ctx_valid(const wclFindContext* ctx)
{
	return static_cast<int>(reinterpret_cast<const la::LinesRepo::FindContext*>(ctx)->isValid());
}

laStrUTF8 la_find_ctx_query(const wclFindContext* ctx)
{
	auto nCtx = reinterpret_cast<const la::LinesRepo::FindContext*>(ctx);
	return convertStr(nCtx->query());
}

int la_find_ctx_line_position(const wclFindContext* ctx, int* lineIndex)
{
	auto nCtx = reinterpret_cast<const la::LinesRepo::FindContext*>(ctx);
	auto pos = nCtx->position();

	if (lineIndex)
		*lineIndex = static_cast<int>(std::get<1>(pos));

	return static_cast<int>(std::get<0>(pos));
}

laStrUTF8* la_list_files(laFlavorType flavor, laStrFixedUTF8 folderPath, int* numFiles)
{
	if (!folderPath.data || (folderPath.size <= 0))
		return nullptr;

	if (!numFiles)
		return nullptr;

	*numFiles = 0;

	auto files = la::LinesRepo::listFolderFiles(static_cast<la::FlavorsRepo::Type>(flavor), { folderPath.data, static_cast<size_t>(folderPath.size) });
	if (files.empty())
		return nullptr;

	auto strList = reinterpret_cast<laStrUTF8*>(std::malloc(sizeof(laStrUTF8) * files.size()));
	if (!strList)
		return nullptr;

	*numFiles = files.size();

	int i = 0;
	for (const auto& str : files)
		strList[i++] = convertStr(str);

	return strList;
}

wclLinesRepo* la_init_repo_file(laFlavorType flavor, laStrFixedUTF8 filePath)
{
	if (!filePath.data || (filePath.size <= 0))
		return nullptr;

	auto repo = la::LinesRepo::initRepoFile(static_cast<la::FlavorsRepo::Type>(flavor), { filePath.data, static_cast<size_t>(filePath.size) });
	return (repo ? reinterpret_cast<wclLinesRepo*>(repo.release()) : nullptr);
}

wclLinesRepo* la_init_repo_folder(laFlavorType flavor, laStrFixedUTF8 folderPath)
{
	if (!folderPath.data || (folderPath.size <= 0))
		return nullptr;

	auto repo = la::LinesRepo::initRepoFolder(static_cast<la::FlavorsRepo::Type>(flavor), { folderPath.data, static_cast<size_t>(folderPath.size) });
	return (repo ? reinterpret_cast<wclLinesRepo*>(repo.release()) : nullptr);
}

wclLinesRepo* la_init_repo_folder_filter(laFlavorType flavor, laStrFixedUTF8 folderPath, laStrFixedUTF8 fileNameFilterRegex)
{
	if (!folderPath.data || (folderPath.size <= 0))
		return nullptr;

	if (!fileNameFilterRegex.data || (fileNameFilterRegex.size <= 0))
		return la_init_repo_folder(flavor, folderPath);

	auto repo = la::LinesRepo::initRepoFolder(static_cast<la::FlavorsRepo::Type>(flavor), { folderPath.data, static_cast<size_t>(folderPath.size) }, { fileNameFilterRegex.data, static_cast<size_t>(fileNameFilterRegex.size) });
	return (repo ? reinterpret_cast<wclLinesRepo*>(repo.release()) : nullptr);
}

wclLinesRepo* la_init_repo_command(wclLinesRepo* repo, laStrFixedUTF8 commandResult)
{
	if (!repo)
		return nullptr;
	if (!commandResult.data || (commandResult.size <= 0))
		return nullptr;

	auto newRepo = la::LinesRepo::initRepoFromCommnand(*reinterpret_cast<la::LinesRepo*>(repo), { commandResult.data, static_cast<size_t>(commandResult.size) });
	return (newRepo ? reinterpret_cast<wclLinesRepo*>(newRepo.release()) : nullptr);
}

void la_repo_destroy(wclLinesRepo** repo)
{
	if (!repo)
		return;

	delete reinterpret_cast<la::LinesRepo*>(*repo);
	*repo = nullptr;
}

int la_repo_num_files(wclLinesRepo* repo)
{
	if (!repo)
		return 0;

	return static_cast<int>(reinterpret_cast<la::LinesRepo*>(repo)->numFiles());
}

int la_repo_num_lines(wclLinesRepo* repo)
{
	if (!repo)
		return 0;

	return static_cast<int>(reinterpret_cast<la::LinesRepo*>(repo)->numLines());
}

laFlavorType la_repo_flavor(wclLinesRepo* repo)
{
	if (!repo)
		return LA_FLAVOR_TYPE_UNKNOWN;

	return static_cast<laFlavorType>(reinterpret_cast<la::LinesRepo*>(repo)->flavor());
}

wclFindContext* la_repo_search_text(wclLinesRepo* repo, laStrFixedUTF8 query, laSearchOptions searchOptions)
{
	if (!repo)
		return nullptr;

	auto fctx = reinterpret_cast<la::LinesRepo*>(repo)->searchText({ query.data, static_cast<size_t>(query.size) }, convertSearchOptions(searchOptions));
	
	auto wrapper = new la::LinesRepo::FindContext(std::move(fctx));
	return (wrapper ? reinterpret_cast<wclFindContext*>(wrapper) : nullptr);
}

wclFindContext* la_repo_search_text_regex(wclLinesRepo* repo, laStrFixedUTF8 query, laSearchOptions searchOptions)
{
	if (!repo)
		return nullptr;

	auto fctx = reinterpret_cast<la::LinesRepo*>(repo)->searchTextRegex({ query.data, static_cast<size_t>(query.size) }, convertSearchOptions(searchOptions));

	auto wrapper = new la::LinesRepo::FindContext(std::move(fctx));
	return (wrapper ? reinterpret_cast<wclFindContext*>(wrapper) : nullptr);
}

void la_repo_search_next(wclLinesRepo* repo, wclFindContext* ctx)
{
	if (!repo || !ctx)
		return;

	auto nCtx = reinterpret_cast<la::LinesRepo::FindContext*>(ctx);
	*nCtx = reinterpret_cast<la::LinesRepo*>(repo)->searchNext(*nCtx);
}

void la_repo_search_destroy(wclFindContext** ctx)
{
	if (!ctx)
		return;

	delete reinterpret_cast<la::LinesRepo::FindContext*>(*ctx);
	*ctx = nullptr;
}

laStrUTF8 la_repo_retrieve_line_content(wclLinesRepo* repo, int lineIndex, laTranslatorType translatorType)
{
	if (!repo || (lineIndex < 0))
		return la_str_init();

	auto res = reinterpret_cast<la::LinesRepo*>(repo)->retrieveLineContent(static_cast<size_t>(lineIndex), static_cast<la::TranslatorsRepo::Type>(translatorType));
	return convertStr(res);
}

laStrUTF8 la_repo_list_available_commands(wclLinesRepo* repo)
{
	if (!repo)
		return la_str_init();

	auto res = reinterpret_cast<la::LinesRepo*>(repo)->listAvailableCommands();
	return convertStr(res);
}

laStrUTF8 la_repo_execute_command(wclLinesRepo* repo, laStrFixedUTF8 tag, laStrFixedUTF8 name)
{
	if (!repo)
		return la_str_init();

	auto res = reinterpret_cast<la::LinesRepo*>(repo)->executeCommand({ tag.data, static_cast<size_t>(tag.size) }, { name.data, static_cast<size_t>(name.size) });
	return convertStr(res);
}

laStrUTF8 la_repo_execute_command_params(wclLinesRepo* repo, laStrFixedUTF8 tag, laStrFixedUTF8 name, laStrFixedUTF8 params)
{
	if (!repo)
		return la_str_init();

	auto res = reinterpret_cast<la::LinesRepo*>(repo)->executeCommand({ tag.data, static_cast<size_t>(tag.size) }, { name.data, static_cast<size_t>(name.size) }, { params.data, static_cast<size_t>(params.size) });
	return convertStr(res);
}

int la_repo_export_lines(wclLinesRepo* repo, const laExportOptions* options, int indexStart, int count)
{
	if (!repo || (indexStart < 0) || (count < 0))
		return 0;

	return reinterpret_cast<la::LinesRepo*>(repo)->exportLines(convertExportOptions(options), static_cast<size_t>(indexStart), static_cast<size_t>(count));
}

int la_repo_export_command_lines(wclLinesRepo* repo, const laExportOptions* options, laStrFixedUTF8 commandResult)
{
	if (!repo || (commandResult.size <= 0))
		return 0;

	return reinterpret_cast<la::LinesRepo*>(repo)->exportCommandLines(convertExportOptions(options), { commandResult.data, static_cast<size_t>(commandResult.size) });
}

int la_repo_export_command_network_packets(wclLinesRepo* repo, const laExportOptions* options, laStrFixedUTF8 commandResult)
{
	if (!repo || (commandResult.size <= 0))
		return 0;

	return reinterpret_cast<la::LinesRepo*>(repo)->exportCommandNetworkPackets(convertExportOptions(options), { commandResult.data, static_cast<size_t>(commandResult.size) });
}
