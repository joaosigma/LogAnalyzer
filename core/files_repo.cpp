#include "files_repo.hpp"

#include <regex>
#include <filesystem>

namespace la
{
	std::vector<std::string> FilesRepo::listFolderFiles(FlavorsRepo::Type type, std::string_view folderPath)
	{
		return FlavorsRepo::listFolderFiles(type, folderPath);
	}

	std::unique_ptr<FilesRepo> FilesRepo::initRepoFile(FlavorsRepo::Type type, std::string_view filePath)
	{
		auto fileMapping = std::make_unique<la::MemoryMappedFile>(filePath);
		if (!(*fileMapping))
			return nullptr;

		auto repo = std::unique_ptr<FilesRepo>{ new FilesRepo(type) };
		repo->m_files.push_back(std::move(fileMapping));

		return repo;
	}

	std::unique_ptr<FilesRepo> FilesRepo::initRepoFolder(FlavorsRepo::Type type, std::string_view folderPath)
	{
		return FilesRepo::initRepoFolder(type, folderPath, {});
	}

	std::unique_ptr<FilesRepo> FilesRepo::initRepoFolder(FlavorsRepo::Type type, std::string_view folderPath, std::string_view fileNameFilterRegex)
	{
		if (fileNameFilterRegex.empty())
		{
			auto repo = std::unique_ptr<FilesRepo>{ new FilesRepo(type) };

			FlavorsRepo::iterateFolderFiles(type, folderPath, [&repo](std::string filePath)
			{
				auto fileMapping = std::make_unique<la::MemoryMappedFile>(filePath);
				if (!(*fileMapping))
					return;

				repo->m_files.push_back(std::move(fileMapping));
			});

			return repo;
		}

		std::regex regFilter;

		try
		{
			regFilter = std::regex{ std::string{ fileNameFilterRegex } };
		}
		catch (std::regex_error const&)
		{
			return nullptr;
		}

		auto repo = std::unique_ptr<FilesRepo>{ new FilesRepo(type) };

		FlavorsRepo::iterateFolderFiles(type, folderPath, [&repo, &regFilter](std::string filePath)
		{
			if (!std::regex_search(filePath, regFilter))
				return;

			auto fileMapping = std::make_unique<la::MemoryMappedFile>(filePath);
			if (!(*fileMapping))
				return;

			repo->m_files.push_back(std::move(fileMapping));
		});

		return repo;
	}
}
