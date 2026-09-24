#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

#include "auction.hpp"
#include "test_framework.hpp"
#include "test_helpers.hpp"

using auction::AuctionResult;
using auction::Order;
using testkit::buy;
using testkit::px;
using testkit::result;
using testkit::sell;

namespace {

AuctionResult run(const std::vector<Order>& orders, std::string_view reference) {
    return auction::runAuction(orders, px(reference));
}

const std::vector<Order> kSpecExample = {
    sell(1527604196773077003, 500, "270.5700"),
    buy(1527604199695788161, 100, "270.3900"),
    sell(1527604199397997988, 100, "0"),
    sell(1527604199974781594, 900, "278.00"),
    buy(1527604200211637272, 100, "0"),
};

}  // namespace

TEST(AuctionSpecExample) {
    CHECK_EQ(run(kSpecExample, "275.99"), result("270.39", 100, 100));
}

TEST(AuctionIsIndependentOfInputOrderWhenTimestampsDiffer) {
    std::vector<Order> reversed(kSpecExample.rbegin(), kSpecExample.rend());
    CHECK_EQ(run(reversed, "275.99"), run(kSpecExample, "275.99"));
}

TEST(AuctionEmptyBookHasNoPrice) {
    CHECK_EQ(run({}, "10"), AuctionResult{});
}

TEST(AuctionOneSidedBookHasNoPrice) {
    CHECK_EQ(run({buy(1, 100, "10"), buy(2, 50, "11"), buy(3, 10, "0")}, "10"), AuctionResult{});
}

// Euronext would display the best bid/ask; the assignment asks for zero.
TEST(EuronextDiffers_NoCross_OutputsZero) {
    CHECK_EQ(run({buy(1, 100, "10"), sell(2, 100, "11")}, "10.5"), AuctionResult{});
}

// Euronext would match at the reference price; the assignment requires an order price.
TEST(EuronextDiffers_MarketOnly_OutputsZero) {
    CHECK_EQ(run({buy(1, 100, "0"), sell(2, 60, "0")}, "10"), AuctionResult{});
}

TEST(AuctionMarketOrdersAreNeverCandidatePrices) {
    CHECK_EQ(run({buy(1, 100, "0"), sell(2, 100, "10")}, "5"), result("10", 100, 0));
}

TEST(AuctionMarketOrdersAreEligibleAtEveryPrice) {
    // At 10: buys 100, sells 50 (market). At 12: no buys.
    CHECK_EQ(run({sell(1, 50, "0"), buy(2, 100, "10"), sell(3, 100, "12")}, "11"), result("10", 50, 50));
}

TEST(AuctionEqualPricesAggregateIntoOneLevel) {
    CHECK_EQ(run({buy(1, 60, "10"), sell(2, 30, "10.0"), buy(3, 40, "10.00"), sell(4, 70, "10")}, "10"),
             result("10", 100, 0));
}

TEST(AuctionRule1_MaximumVolumeBeatsReference) {
    // At 9: volume 50. At 10: volume 100, imbalance -50. Reference favours 9.
    CHECK_EQ(run({buy(1, 100, "10"), sell(2, 50, "9"), sell(3, 100, "10")}, "9"), result("10", 100, -50));
}

TEST(AuctionRule2_MinimumAbsoluteImbalance) {
    // At 9: volume 100, imbalance +20. At 10: volume 100, imbalance -30.
    // The signed minimum and the reference would both pick 10.
    const std::vector<Order> orders = {buy(1, 20, "9"), buy(2, 100, "10"), sell(3, 100, "9"), sell(4, 30, "10")};
    CHECK_EQ(run(orders, "10"), result("9", 100, 20));
}

TEST(AuctionRule3_ClosestToReference) {
    // Volume 100 and imbalance 0 at both 9 and 10.
    const std::vector<Order> orders = {buy(1, 100, "10"), sell(2, 100, "9")};
    CHECK_EQ(run(orders, "9.9"), result("10", 100, 0));
    CHECK_EQ(run(orders, "9.2"), result("9", 100, 0));
    CHECK_EQ(run(orders, "50"), result("10", 100, 0));
    CHECK_EQ(run(orders, "1"), result("9", 100, 0));
}

// Euronext's documented rules stop at the reference price; the assignment
// adds the oldest-eligible-order rule.
TEST(EuronextDiffers_FinalTieBreak_OldestBuyPicksLowest) {
    CHECK_EQ(run({buy(1, 100, "10"), sell(2, 100, "9")}, "9.5"), result("9", 100, 0));
}

TEST(EuronextDiffers_FinalTieBreak_OldestSellPicksHighest) {
    CHECK_EQ(run({buy(2, 100, "10"), sell(1, 100, "9")}, "9.5"), result("10", 100, 0));
}

TEST(EuronextDiffers_FinalTieBreak_ReportsImbalanceOfChosenPrice) {
    // At 9: imbalance +10. At 10: imbalance -10. Volume 100 at both.
    const auto book = [](std::uint64_t buyTime, std::uint64_t sellTime) {
        return std::vector<Order>{buy(buyTime, 100, "10"), buy(5, 10, "9"), sell(sellTime, 100, "9"),
                                  sell(6, 10, "10")};
    };
    CHECK_EQ(run(book(1, 2), "9.5"), result("9", 100, 10));
    CHECK_EQ(run(book(2, 1), "9.5"), result("10", 100, -10));
}

TEST(EuronextDiffers_FinalTieBreak_IgnoresOrdersIneligibleAtTiedPrices) {
    // The buy at 1 and the sell at 50 are older but not eligible at 9 or 10.
    const std::vector<Order> orders = {buy(1, 1, "1"), sell(0, 1, "50"), buy(5, 100, "10"), sell(3, 100, "9")};
    CHECK_EQ(run(orders, "9.5"), result("10", 100, 0));
}

TEST(EuronextDiffers_FinalTieBreak_UsesTimestampNotFileOrder) {
    CHECK_EQ(run({buy(200, 100, "10"), sell(100, 100, "9")}, "9.5"), result("10", 100, 0));
}

TEST(EuronextDiffers_FinalTieBreak_EqualTimestampsUseFileOrder) {
    CHECK_EQ(run({buy(100, 100, "10"), sell(100, 100, "9")}, "9.5"), result("9", 100, 0));
    CHECK_EQ(run({sell(100, 100, "9"), buy(100, 100, "10")}, "9.5"), result("10", 100, 0));
}

TEST(EuronextDiffers_FinalTieBreak_MarketOrderCanBeOldest) {
    CHECK_EQ(run({sell(1, 50, "0"), buy(2, 150, "10"), sell(3, 100, "9")}, "9.5"), result("10", 150, 0));
    CHECK_EQ(run({buy(1, 50, "0"), buy(2, 100, "10"), sell(3, 150, "9")}, "9.5"), result("9", 150, 0));
}

TEST(AuctionLargeQuantitiesDoNotOverflow) {
    constexpr std::int64_t kHalf = INT64_MAX / 2;
    CHECK_EQ(run({buy(1, kHalf, "0"), sell(2, kHalf, "10")}, "10"), result("10", kHalf, 0));
    CHECK_EQ(run({buy(1, INT64_MAX - 1, "10"), sell(2, 1, "10")}, "10"), result("10", 1, INT64_MAX - 2));
}
