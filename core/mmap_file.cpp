#include "mmap_file.hpp"

#if defined(_WIN32) || defined(WIN32)
	#include <windows.h>
#else
	#include <fcntl.h>
	#include <unistd.h>
	#include <sys/mman.h>
	#include <sys/stat.h>
	#include <sys/types.h>
#endif

#include <filesystem>

namespace la
{
#if defined(_WIN32) || defined(WIN32)
	class MemoryMappedFile::Impl
	{
	public:
		Impl(std::string_view filePath)
		{
			{
				auto path = std::filesystem::u8path(filePath);
				if (!std::filesystem::is_regular_file(path))
					return;

				m_hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (m_hFile == INVALID_HANDLE_VALUE)
					return;
			}

			m_hFileMapping = CreateFileMappingW(m_hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
			if (!m_hFileMapping)
				return;

			m_fileData = MapViewOfFile(m_hFileMapping, FILE_MAP_READ, 0, 0, 0);

			{
				MEMORY_BASIC_INFORMATION memInfo;
				std::memset(&memInfo, 0, sizeof(MEMORY_BASIC_INFORMATION));

				if (VirtualQuery(m_fileData, &memInfo, sizeof(MEMORY_BASIC_INFORMATION)) != sizeof(MEMORY_BASIC_INFORMATION))
					return;

				m_fileSize = static_cast<size_t>(memInfo.RegionSize);
			}
		}

		~Impl() noexcept
		{
			if (m_fileData)
				UnmapViewOfFile(m_fileData);

			if (m_hFileMapping)
				CloseHandle(m_hFileMapping);

			if (m_hFile != INVALID_HANDLE_VALUE)
				CloseHandle(m_hFile);
		}

		explicit operator bool() const noexcept { return (m_fileSize > 0); }

		size_t size() const noexcept { return m_fileSize; }
		const void* data() const noexcept { return m_fileData; }

	private:
		size_t m_fileSize{ 0 };
		const void* m_fileData{ nullptr };

		HANDLE m_hFile{ INVALID_HANDLE_VALUE };
		HANDLE m_hFileMapping{ nullptr };
	};
#else
	class MemoryMappedFile::Impl
	{
	public:
		Impl(std::string_view filePath)
		{
			{
				auto path = std::filesystem::u8path(filePath);
				if (!std::filesystem::is_regular_file(path))
					return;

				m_fd = open(path.c_str(), O_RDONLY);
				if (m_fd < 0)
					return;
			}

			{
				struct stat s;
				if (fstat(m_fd, &s) < 0)
					return;

				m_fileSize = s.st_size;
			}

			m_fileData = mmap(nullptr, m_fileSize, PROT_READ, MAP_SHARED, m_fd, 0);
			if (m_fileData == MAP_FAILED)
			{
				m_fileSize = 0;
				m_fileData = nullptr;
				return;
			}
		}

		~Impl() noexcept
		{
			if (m_fileData)
				munmap(m_fileData, m_fileSize);

			if (m_fd != -1)
				close(m_fd);
		}

		explicit operator bool() const noexcept { return (m_fileSize > 0); }

		size_t size() const noexcept { return m_fileSize; }
		const void* data() const noexcept { return m_fileData; }

	private:
		int m_fd{ -1 };
		size_t m_fileSize;
		void* m_fileData{ nullptr };
	};
#endif

	MemoryMappedFile::MemoryMappedFile(std::string_view filePath)
	{
		m_impl = std::make_unique<Impl>(filePath);
	}

	MemoryMappedFile::~MemoryMappedFile()
	{ }

	MemoryMappedFile::operator bool() const noexcept
	{
		return m_impl->operator bool();
	}

	size_t MemoryMappedFile::size() const noexcept
	{
		return m_impl->size();
	}

	const void* MemoryMappedFile::data() const noexcept
	{
		return m_impl->data();
	}
}
