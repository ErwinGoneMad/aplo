#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace auction {

// Provides a read-only contiguous view of an input file. POSIX builds use a
// private mmap; other platforms fall back to an owned string buffer.
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
    std::string fallback_;
};

}  // namespace auction
