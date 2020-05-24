#ifndef LA_FILES_REPO_HPP
#define LA_FILES_REPO_HPP

#include "mmap_file.hpp"
#include "flavors_repo.hpp"

#include <memory>
#include <string>
#include <vector>

namespace la
{
	class FilesRepo final
	{
	public:
		static std::vector<std::string> listFolderFiles(FlavorsRepo::Type type, std::string_view folderPath);

		static std::unique_ptr<FilesRepo> initRepoFile(FlavorsRepo::Type type, std::string_view filePath);
		static std::unique_ptr<FilesRepo> initRepoFolder(FlavorsRepo::Type type, std::string_view folderPath);
		static std::unique_ptr<FilesRepo> initRepoFolder(FlavorsRepo::Type type, std::string_view folderPath, std::string_view fileNameFilterRegex);

	public:
		~FilesRepo() = default;

		FilesRepo(const FilesRepo&) = delete;
		FilesRepo& operator=(const FilesRepo&) = delete;
		FilesRepo(FilesRepo&&) = default;
		FilesRepo& operator=(FilesRepo&&) = default;

		FlavorsRepo::Type flavor() const noexcept
		{
			return m_flavor;
		}

		size_t numFiles() const noexcept
		{
			return m_files.size();
		}

		template<class TCallback>
		void iterateFiles(TCallback&& cb) const noexcept
		{
			for (const auto& file : m_files)
				cb(file->data(), file->size());
		}

	private:
		FilesRepo(FlavorsRepo::Type flavor) noexcept
			: m_flavor{ flavor }
		{ }

	private:
		FlavorsRepo::Type m_flavor{ FlavorsRepo::Type::Unknown };
		std::vector<std::unique_ptr<MemoryMappedFile>> m_files;
	};
}

#endif
