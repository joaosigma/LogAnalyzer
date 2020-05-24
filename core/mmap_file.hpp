#ifndef LA_MMAP_FILES_HPP
#define LA_MMAP_FILES_HPP

#include <memory>
#include <string>

namespace la
{
	class MemoryMappedFile
	{
		class Impl;

	public:
		MemoryMappedFile(std::string_view filePath);
		~MemoryMappedFile();

		MemoryMappedFile(const MemoryMappedFile&) = delete;
		MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;
		MemoryMappedFile(MemoryMappedFile&&) = default;
		MemoryMappedFile& operator=(MemoryMappedFile&&) = default;

		explicit operator bool() const noexcept;

		size_t size() const noexcept;
		const void* data() const noexcept;

		template<class T>
		T dataAs() const noexcept
		{
			return reinterpret_cast<T>(data());
		}

	private:
		std::unique_ptr<Impl> m_impl;
	};
}

#endif
