#include "api.h"

#include <lines_repo.hpp>

const char** la_list_files(laFlavorType flavor, const char* folderPath, int* numFiles)
{
	if (!numFiles)
		return nullptr;

	auto files = la::LinesRepo::listFolderFiles(static_cast<la::FlavorsRepo::Type>(flavor), { folderPath, std::strlen(folderPath) });

	//TODO
	return nullptr;
}

wclLinesRepo* la_init_repo_file(laFlavorType flavor, const char* filePath)
{
	if (!filePath)
		return nullptr;

	auto repo = la::LinesRepo::initRepoFile(static_cast<la::FlavorsRepo::Type>(flavor), { filePath, std::strlen(filePath) });
	return (repo ? reinterpret_cast<wclLinesRepo*>(repo.release()) : nullptr);
}

wclLinesRepo* la_init_repo_folder(laFlavorType flavor, const char* folderPath)
{
	if (!folderPath)
		return nullptr;

	auto repo = la::LinesRepo::initRepoFolder(static_cast<la::FlavorsRepo::Type>(flavor), { folderPath, std::strlen(folderPath) });
	return (repo ? reinterpret_cast<wclLinesRepo*>(repo.release()) : nullptr);
}

wclLinesRepo* la_init_repo_folder_filter(laFlavorType flavor, const char* folderPath, const char* fileNameFilterRegex)
{
	if (!folderPath)
		return nullptr;

	if (!fileNameFilterRegex || (*fileNameFilterRegex == '\0'))
		return la_init_repo_folder(flavor, folderPath);

	auto repo = la::LinesRepo::initRepoFolder(static_cast<la::FlavorsRepo::Type>(flavor), { folderPath, std::strlen(folderPath) }, { fileNameFilterRegex, std::strlen(fileNameFilterRegex) });
	return (repo ? reinterpret_cast<wclLinesRepo*>(repo.release()) : nullptr);
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

void la_repo_destroy(wclLinesRepo** repo)
{
	if (!repo)
		return;

	delete reinterpret_cast<la::LinesRepo*>(repo);
	*repo = nullptr;
}
