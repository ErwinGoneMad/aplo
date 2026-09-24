#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#include "auction.hpp"
#include "brute_force.hpp"
#include "test_framework.hpp"
#include "test_helpers.hpp"

using auction::Order;
using auction::Price;
using auction::Side;

namespace {

constexpr std::uint64_t kSeed = 20260923;
constexpr int kIterations = 20'000;
constexpr std::int64_t kBaseTicks = 100 * Price::kScale;  // 100.00
constexpr std::int64_t kStepTicks = Price::kScale / 100;  // 0.01

struct Scenario {
    std::vector<Order> orders;
    Price reference;
};

struct Shape {
    std::int64_t maxOrders;
    std::int64_t maxGridSteps;
};

// Small books keep ties frequent, so every selection rule is exercised;
// larger books cover longer level scans and cumulative sums.
constexpr Shape kSmallBooks{.maxOrders = 16, .maxGridSteps = 5};
constexpr Shape kLargeBooks{.maxOrders = 60, .maxGridSteps = 30};

// Books on a price grid with few distinct quantities and timestamps. Most
// reference prices sit midway between two grid points, which creates
// equidistant ties; the rest land anywhere around the grid, including outside it.
struct ScenarioGenerator {
    std::mt19937_64 rng{kSeed};

    std::int64_t uniform(std::int64_t low, std::int64_t high) {
        return std::uniform_int_distribution<std::int64_t>{low, high}(rng);
    }

    Price gridPrice(std::int64_t halfSteps) { return Price::fromTicks(kBaseTicks + halfSteps * kStepTicks / 2); }

    Scenario next(const Shape& shape) {
        const std::int64_t gridSteps = uniform(1, shape.maxGridSteps);
        const std::int64_t maxQuantity = uniform(1, 6);

        Scenario scenario;
        scenario.orders.resize(static_cast<std::size_t>(uniform(0, shape.maxOrders)));
        for (Order& order : scenario.orders) {
            const bool isMarket = uniform(0, 9) == 0;
            order = Order{
                .timestamp = static_cast<std::uint64_t>(uniform(1, shape.maxOrders)),
                .quantity = uniform(1, maxQuantity),
                .price = isMarket ? Price{} : gridPrice(2 * uniform(0, gridSteps)),
                .side = uniform(0, 1) == 0 ? Side::Buy : Side::Sell,
            };
        }
        scenario.reference = uniform(0, 9) < 7 ? gridPrice(uniform(0, gridSteps) + uniform(0, gridSteps))
                                               : gridPrice(uniform(-4, 2 * gridSteps + 4));
        return scenario;
    }
};

}  // namespace

namespace {

// Returns how many scenarios needed the final tie-break.
int compareWithBruteForce(ScenarioGenerator& generator, const Shape& shape) {
    int finalTieBreaks = 0;
    int mismatches = 0;
    for (int i = 0; i < kIterations && mismatches < 5; ++i) {
        const auto [orders, reference] = generator.next(shape);

        const testkit::BruteForceOutcome expected = testkit::bruteForceAuction(orders, reference);
        const auction::AuctionResult actual = auction::runAuction(orders, reference);
        if (expected.usedFinalTieBreak) ++finalTieBreaks;
        if (!(actual == expected.result)) {
            ++mismatches;
            CHECK_EQ(actual, expected.result);
            std::cerr << "      iteration " << i << ", seed " << kSeed << ", " << orders.size() << " orders\n";
        }
    }
    return finalTieBreaks;
}

}  // namespace

TEST(DifferentialAgainstBruteForce_SmallBooks) {
    ScenarioGenerator generator;
    const int finalTieBreaks = compareWithBruteForce(generator, kSmallBooks);
    // Guards the generator itself: the rarest rule must actually be exercised.
    CHECK(finalTieBreaks > kIterations / 100);
}

TEST(DifferentialAgainstBruteForce_LargeBooks) {
    ScenarioGenerator generator;
    compareWithBruteForce(generator, kLargeBooks);
}

TEST(DifferentialDeterministicReplay) {
    // With distinct timestamps the input order carries no information, so any
    // permutation of the same orders must produce the same result.
    ScenarioGenerator generator;
    for (int i = 0; i < 2'000; ++i) {
        auto [orders, reference] = generator.next(kLargeBooks);
        for (std::size_t k = 0; k < orders.size(); ++k) orders[k].timestamp = k + 1;

        const auction::AuctionResult first = auction::runAuction(orders, reference);
        std::shuffle(orders.begin(), orders.end(), generator.rng);
        CHECK_EQ(auction::runAuction(orders, reference), first);
        CHECK_EQ(auction::runAuction(orders, reference), first);
    }
}
