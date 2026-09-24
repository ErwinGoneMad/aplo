// Times parseOrders and runAuction separately on a synthetic book.
//
// Usage: auction_bench [orders=1000000] [price_levels=2001] [repetitions=21]
//
// The book is generated once from a fixed seed, so runs are comparable. One
// untimed warm-up precedes the timed repetitions, and the distribution
// (min, median, p90, max) is reported rather than a mean, because tail
// latency matters more than the average.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "auction.hpp"
#include "parser.hpp"
#include "price.hpp"

namespace {

using Clock = std::chrono::steady_clock;

struct Config {
    std::size_t orders = 1'000'000;
    std::int64_t priceLevels = 2'001;
    std::size_t repetitions = 21;
};

std::size_t argumentOr(int argc, char** argv, int index, std::size_t fallback) {
    if (index >= argc) return fallback;
    const long long value = std::atoll(argv[index]);
    return value > 0 ? static_cast<std::size_t>(value) : fallback;
}

// Prices on a 0.01 tick, centred on 100.00 when the level count allows it;
// 5% market orders.
std::string generateInput(const Config& config) {
    std::mt19937_64 rng{42};
    std::uniform_int_distribution<std::int64_t> level{0, config.priceLevels - 1};
    std::uniform_int_distribution<int> quantity{1, 1'000};
    std::uniform_int_distribution<int> percent{0, 99};
    const std::int64_t lowestCents = std::max<std::int64_t>(1, 10'000 - config.priceLevels / 2);

    std::ostringstream out;
    std::uint64_t timestamp = 1'527'604'196'773'077'003;
    for (std::size_t i = 0; i < config.orders; ++i) {
        timestamp += static_cast<std::uint64_t>(percent(rng)) + 1;
        const char side = percent(rng) < 50 ? 'B' : 'S';
        out << timestamp << ",AAPL," << side << ',' << quantity(rng) << ',';
        if (percent(rng) < 5) {
            out << '0';
        } else {
            const std::int64_t cents = lowestCents + level(rng);
            out << cents / 100 << '.' << std::setw(2) << std::setfill('0') << cents % 100;
        }
        out << '\n';
    }
    return out.str();
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

void report(const char* name, const std::vector<double>& samples, std::size_t orders) {
    const auto at = [&](double quantile) {
        return samples[static_cast<std::size_t>(quantile * static_cast<double>(samples.size() - 1))];
    };
    std::cout << std::fixed << std::setprecision(2) << std::setfill(' ') << std::left << std::setw(10) << name
              << " min " << at(0.0) << " ms, median " << at(0.5) << " ms, p90 " << at(0.9) << " ms, max "
              << at(1.0) << " ms, median " << at(0.5) * 1e6 / static_cast<double>(orders) << " ns/order\n";
}

}  // namespace

int main(int argc, char** argv) {
    Config config;
    config.orders = argumentOr(argc, argv, 1, config.orders);
    config.priceLevels = static_cast<std::int64_t>(argumentOr(argc, argv, 2, 2'001));
    config.repetitions = argumentOr(argc, argv, 3, config.repetitions);

    const std::string input = generateInput(config);
    std::cout << config.orders << " orders, " << config.priceLevels << " price levels, " << config.repetitions
              << " timed repetitions after 1 warm-up\n";

    auction::OrderFile file;
    const auto parseSamples = timeMilliseconds(config.repetitions, [&] {
        std::istringstream in(input);
        file = auction::parseOrders(in, "generated");
    });

    const auction::Price reference = *auction::parsePrice("100.00");
    auction::AuctionResult result;
    const auto auctionSamples =
        timeMilliseconds(config.repetitions, [&] { result = auction::runAuction(file.orders, reference); });

    report("parse", parseSamples, config.orders);
    report("auction", auctionSamples, config.orders);
    std::cout << "result: price " << result.price << ", volume " << result.volume << ", imbalance "
              << result.imbalance << '\n';
    return 0;
}
