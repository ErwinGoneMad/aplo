// Compares three ways of acquiring and parsing the same cached CSV file:
//   1. std::ifstream + std::getline (the production baseline)
//   2. std::ifstream::read into contiguous memory + direct scanning
//   3. mmap + direct scanning
//
// Usage: parser_bench [orders=1000000] [repetitions=21]
//                     [all|getline|buffered|mmap]

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "parser.hpp"

namespace {

using Clock = std::chrono::steady_clock;

struct Config {
    std::size_t orders = 1'000'000;
    std::size_t repetitions = 21;
};

class FileDescriptor {
public:
    explicit FileDescriptor(int value = -1) : value_{value} {}
    ~FileDescriptor() {
        if (value_ >= 0) ::close(value_);
    }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(FileDescriptor&& other) noexcept : value_{std::exchange(other.value_, -1)} {}
    int get() const { return value_; }

private:
    int value_;
};

class Mapping {
public:
    Mapping(void* address, std::size_t size) : address_{address}, size_{size} {}
    ~Mapping() {
        if (address_ != MAP_FAILED) ::munmap(address_, size_);
    }
    Mapping(const Mapping&) = delete;
    Mapping& operator=(const Mapping&) = delete;
    const char* data() const { return static_cast<const char*>(address_); }

private:
    void* address_;
    std::size_t size_;
};

class TemporaryFile {
public:
    explicit TemporaryFile(std::string contents) {
        char pattern[] = "/tmp/aplo-parser-bench.XXXXXX";
        FileDescriptor fd{::mkstemp(pattern)};
        if (fd.get() < 0) throw std::runtime_error(std::strerror(errno));
        path_ = pattern;

        const char* data = contents.data();
        std::size_t remaining = contents.size();
        while (remaining != 0) {
            const ssize_t written = ::write(fd.get(), data, remaining);
            if (written < 0) throw std::runtime_error(std::strerror(errno));
            const auto count = static_cast<std::size_t>(written);
            data += count;
            remaining -= count;
        }
    }

    ~TemporaryFile() {
        if (!path_.empty()) std::remove(path_.c_str());
    }
    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    const std::string& path() const { return path_; }

private:
    std::string path_;
};

std::size_t argumentOr(int argc, char** argv, int index, std::size_t fallback) {
    if (index >= argc) return fallback;
    const long long value = std::atoll(argv[index]);
    return value > 0 ? static_cast<std::size_t>(value) : fallback;
}

std::string generateInput(std::size_t orderCount) {
    std::mt19937_64 rng{42};
    std::uniform_int_distribution<int> quantity{1, 1'000};
    std::uniform_int_distribution<int> cents{1, 20'000};
    std::uniform_int_distribution<int> percent{0, 99};

    std::ostringstream out;
    std::uint64_t timestamp = 1'527'604'196'773'077'003;
    for (std::size_t i = 0; i < orderCount; ++i) {
        timestamp += static_cast<std::uint64_t>(percent(rng)) + 1;
        const char side = percent(rng) < 50 ? 'B' : 'S';
        out << timestamp << ",AAPL," << side << ',' << quantity(rng) << ',';
        if (percent(rng) < 5) {
            out << '0';
        } else {
            const int price = cents(rng);
            out << price / 100 << '.' << std::setw(2) << std::setfill('0') << price % 100;
        }
        out << '\n';
    }
    return out.str();
}

auction::OrderFile parseGetline(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open benchmark input");
    return auction::parseOrders(in, path);
}

auction::OrderFile parseBuffered(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("cannot open benchmark input");
    const std::streampos end = in.tellg();
    if (end < 0) throw std::runtime_error("cannot determine benchmark input size");
    const auto size = static_cast<std::size_t>(end);
    std::string input(size, '\0');
    in.seekg(0);
    if (size != 0 && !in.read(input.data(), static_cast<std::streamsize>(size))) {
        throw std::runtime_error("cannot read benchmark input");
    }
    return auction::parseOrders(std::string_view{input}, path);
}

auction::OrderFile parseMmap(const std::string& path) {
    FileDescriptor fd{::open(path.c_str(), O_RDONLY)};
    if (fd.get() < 0) throw std::runtime_error(std::strerror(errno));

    struct stat status {};
    if (::fstat(fd.get(), &status) != 0) throw std::runtime_error(std::strerror(errno));
    if (status.st_size == 0) return {};
    const auto size = static_cast<std::size_t>(status.st_size);
    Mapping mapping{::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd.get(), 0), size};
    if (mapping.data() == static_cast<const char*>(MAP_FAILED)) {
        throw std::runtime_error(std::strerror(errno));
    }
    return auction::parseOrders(std::string_view{mapping.data(), size}, path);
}

template <typename Body>
std::vector<double> timeMilliseconds(std::size_t repetitions, Body body) {
    body();
    std::vector<double> samples;
    samples.reserve(repetitions);
    for (std::size_t i = 0; i < repetitions; ++i) {
        const auto start = Clock::now();
        body();
        samples.push_back(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples;
}

void report(std::string_view name, const std::vector<double>& samples, std::size_t orders) {
    const auto at = [&](double quantile) {
        return samples[static_cast<std::size_t>(quantile * static_cast<double>(samples.size() - 1))];
    };
    std::cout << std::fixed << std::setprecision(2) << std::setfill(' ') << std::left << std::setw(14) << name
              << " min " << at(0.0) << " ms, median " << at(0.5) << " ms, p90 " << at(0.9)
              << " ms, max " << at(1.0) << " ms, median " << at(0.5) * 1e6 / static_cast<double>(orders)
              << " ns/order\n";
}

void verifyEquivalent(const auction::OrderFile& expected, const auction::OrderFile& actual) {
    if (expected.symbol != actual.symbol || expected.orders.size() != actual.orders.size()) {
        throw std::runtime_error("parser results differ");
    }
    for (std::size_t i = 0; i < expected.orders.size(); ++i) {
        const auto& a = expected.orders[i];
        const auto& b = actual.orders[i];
        if (a.timestamp != b.timestamp || a.quantity != b.quantity || a.price != b.price || a.side != b.side) {
            throw std::runtime_error("parser results differ at order " + std::to_string(i));
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const Config config{
        .orders = argumentOr(argc, argv, 1, 1'000'000),
        .repetitions = argumentOr(argc, argv, 2, 21),
    };
    const std::string_view selected = argc >= 4 ? argv[3] : "all";
    if (selected != "all" && selected != "getline" && selected != "buffered" && selected != "mmap") {
        std::cerr << "method must be all, getline, buffered, or mmap\n";
        return 2;
    }
    const std::string input = generateInput(config.orders);
    const TemporaryFile file{input};

    const auction::OrderFile expected = parseGetline(file.path());
    verifyEquivalent(expected, parseBuffered(file.path()));
    verifyEquivalent(expected, parseMmap(file.path()));

    std::cout << config.orders << " orders, " << input.size() << " input bytes, " << config.repetitions
              << " timed repetitions after 1 warm-up; file cache is warm\n";

    auction::OrderFile result;
    if (selected == "all" || selected == "getline") {
        report("getline", timeMilliseconds(config.repetitions, [&] { result = parseGetline(file.path()); }),
               config.orders);
    }
    if (selected == "all" || selected == "buffered") {
        report("buffered", timeMilliseconds(config.repetitions, [&] { result = parseBuffered(file.path()); }),
               config.orders);
    }
    if (selected == "all" || selected == "mmap") {
        report("mmap", timeMilliseconds(config.repetitions, [&] { result = parseMmap(file.path()); }),
               config.orders);
    }

    std::cout << "verified: all methods produced " << result.orders.size() << " identical orders for symbol "
              << result.symbol << '\n';
    return 0;
}
