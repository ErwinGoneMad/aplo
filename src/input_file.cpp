#include "input_file.hpp"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace auction {

namespace {

std::runtime_error fileError(const std::string& path, std::string_view reason) {
    return std::runtime_error("cannot read '" + path + "': " + std::string(reason));
}

class FileDescriptor {
public:
    explicit FileDescriptor(int value) : value_{value} {}
    ~FileDescriptor() {
        if (value_ >= 0) ::close(value_);
    }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    int get() const noexcept { return value_; }

private:
    int value_;
};

}  // namespace

InputFile::InputFile(const std::string& path) {
    FileDescriptor file{::open(path.c_str(), O_RDONLY)};
    if (file.get() < 0) throw fileError(path, std::strerror(errno));

    struct stat status {};
    if (::fstat(file.get(), &status) != 0) throw fileError(path, std::strerror(errno));
    if (!S_ISREG(status.st_mode)) throw fileError(path, "not a regular file");
    if (status.st_size < 0 || static_cast<std::uintmax_t>(status.st_size) >
                                  static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        throw fileError(path, "file is too large");
    }

    size_ = static_cast<std::size_t>(status.st_size);
    if (size_ == 0) return;

    mapping_ = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, file.get(), 0);
    if (mapping_ == MAP_FAILED) {
        mapping_ = nullptr;
        throw fileError(path, std::strerror(errno));
    }
}

InputFile::~InputFile() {
    if (mapping_ != nullptr) ::munmap(mapping_, size_);
}

std::string_view InputFile::bytes() const noexcept {
    if (mapping_ == nullptr) return {};
    return {static_cast<const char*>(mapping_), size_};
}

}  // namespace auction
