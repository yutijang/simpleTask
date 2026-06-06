#pragma once

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

class UniqueFd {
  public:
    explicit UniqueFd(int const fd = -1) noexcept : m_fd(fd) {}

    ~UniqueFd() noexcept {
        cleanup();
    }

    UniqueFd(UniqueFd const&) = delete;
    UniqueFd& operator=(UniqueFd const&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : m_fd(std::exchange(other.m_fd, -1)) {}

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            cleanup();

            m_fd = std::exchange(other.m_fd, -1);
        }

        return *this;
    }

    [[nodiscard]]
    int get() const noexcept {
        return m_fd;
    }

    [[nodiscard]]
    int release() noexcept {
        return std::exchange(m_fd, -1);
    }

    void reset(int const newFd = -1) noexcept {
        cleanup();
        m_fd = newFd;
    }

    explicit operator bool() const noexcept {
        return m_fd != -1;
    }

  private:
    void cleanup() noexcept {
        if (m_fd != -1) {
            ::close(m_fd);
            m_fd = -1;
        }
    }

    int m_fd{-1};
};

class [[nodiscard]] MemoryMappedFile {
  public:
    explicit MemoryMappedFile(std::filesystem::path const& path) {
        UniqueFd fd{::open(path.c_str(), O_RDONLY)};
        if (!fd) {
            throw std::runtime_error(std::string("error: ") + std::strerror(errno));
        }

        struct stat statBuffer{};
        if (::fstat(fd.get(), &statBuffer) == -1) {
            throw std::runtime_error(std::string("error: ") + std::strerror(errno));
        }

        m_size = static_cast<std::size_t>(statBuffer.st_size);
        if (m_size == 0) {
            throw std::runtime_error("empty file");
        }

        m_mapping = ::mmap(nullptr, m_size, PROT_READ, MAP_SHARED, fd.get(), 0);
        if (m_mapping == MAP_FAILED) {
            throw std::runtime_error(std::string("error: ") + std::strerror(errno));
        }
    }

    ~MemoryMappedFile() noexcept {
        cleanup();
    }

    MemoryMappedFile(MemoryMappedFile const&) = delete;
    MemoryMappedFile& operator=(MemoryMappedFile const&) = delete;

    MemoryMappedFile(MemoryMappedFile&& other) noexcept
        : m_mapping{std::exchange(other.m_mapping, MAP_FAILED)},
          m_size{std::exchange(other.m_size, 0)} {}

    MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept {
        if (this != &other) {
            cleanup();

            m_mapping = std::exchange(other.m_mapping, MAP_FAILED);
            m_size = std::exchange(other.m_size, 0);
        }

        return *this;
    }

    [[nodiscard]]
    std::byte const* data() const noexcept {
        return static_cast<std::byte const*>(m_mapping);
    }

    [[nodiscard]]
    std::size_t size() const noexcept {
        return m_size;
    }

  private:
    void cleanup() noexcept {
        if (m_mapping != MAP_FAILED) {
            ::munmap(m_mapping, m_size);

            m_mapping = MAP_FAILED;
            m_size = 0;
        }
    }

    void* m_mapping{MAP_FAILED};
    std::size_t m_size{};
};
