#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace auction {

// Provides a read-only contiguous view of a regular file using a private mmap.
class InputFile {
public:
    explicit InputFile(const std::string& path);
    ~InputFile();

    InputFile(const InputFile&) = delete;
    InputFile& operator=(const InputFile&) = delete;

    std::string_view bytes() const noexcept;

private:
    void* mapping_ = nullptr;
    std::size_t size_ = 0;
};

}  // namespace auction
