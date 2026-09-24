#include "auction.hpp"

#include <algorithm>
#include <cassert>
#include <compare>
#include <cstdlib>
#include <optional>
#include <vector>

namespace auction {

namespace {

struct Level {
    Price price;
    std::int64_t buyQuantity = 0;
    std::int64_t sellQuantity = 0;
};

struct Book {
    std::vector<Level> levels;  // one per distinct limit price, ascending
    std::int64_t marketBuy = 0;
    std::int64_t marketSell = 0;
    std::int64_t limitBuy = 0;
};

struct Candidate {
    Price price;
    std::int64_t volume = 0;
    std::int64_t imbalance = 0;
    std::int64_t distance = 0;  // |price - reference| in ticks
};

Book buildBook(std::span<const Order> orders) {
    Book book;
    book.levels.reserve(orders.size());
    for (const Order& order : orders) {
        const bool isBuy = order.side == Side::Buy;
        if (order.price.isMarket()) {
            (isBuy ? book.marketBuy : book.marketSell) += order.quantity;
            continue;
        }
        if (isBuy) book.limitBuy += order.quantity;
        book.levels.push_back(Level{
            .price = order.price,
            .buyQuantity = isBuy ? order.quantity : 0,
            .sellQuantity = isBuy ? 0 : order.quantity,
        });
    }

    std::sort(book.levels.begin(), book.levels.end(),
              [](const Level& a, const Level& b) { return a.price < b.price; });

    std::size_t merged = 0;
    for (const Level& level : book.levels) {
        if (merged > 0 && book.levels[merged - 1].price == level.price) {
            book.levels[merged - 1].buyQuantity += level.buyQuantity;
            book.levels[merged - 1].sellQuantity += level.sellQuantity;
        } else {
            book.levels[merged++] = level;
        }
    }
    book.levels.resize(merged);
    return book;
}

// `less` means `a` ranks better than `b`; `equal` means the first three
// auction rules cannot separate them.
std::strong_ordering compareRank(const Candidate& a, const Candidate& b) {
    if (const auto byVolume = b.volume <=> a.volume; byVolume != 0) return byVolume;
    if (const auto byImbalance = std::abs(a.imbalance) <=> std::abs(b.imbalance); byImbalance != 0) {
        return byImbalance;
    }
    return a.distance <=> b.distance;
}

// The oldest order eligible at any price in [low, high]: every market order,
// buys priced at or above `low` and sells priced at or below `high`.
// Whichever side it is on, it remains eligible at the price it selects.
Side oldestEligibleSide(std::span<const Order> orders, Price low, Price high) {
    const Order* oldest = nullptr;
    for (const Order& order : orders) {
        const bool eligible = order.price.isMarket() ||
                              (order.side == Side::Buy ? order.price >= low : order.price <= high);
        if (eligible && (oldest == nullptr || order.timestamp < oldest->timestamp)) oldest = &order;
    }
    assert(oldest != nullptr);
    return oldest->side;
}

AuctionResult toResult(const Candidate& candidate) {
    return AuctionResult{
        .price = candidate.price,
        .volume = candidate.volume,
        .imbalance = candidate.imbalance,
    };
}

}  // namespace

AuctionResult runAuction(std::span<const Order> orders, Price reference) {
    const Book book = buildBook(orders);

    // Levels are visited in ascending price order, so `tied`, when set, is
    // always priced above `best`. Distinct prices at equal distance from the
    // reference number at most two, so no third level can tie with both.
    std::optional<Candidate> best;
    std::optional<Candidate> tied;
    std::int64_t buysAtOrAbove = book.marketBuy + book.limitBuy;
    std::int64_t sellsAtOrBelow = book.marketSell;

    for (const Level& level : book.levels) {
        sellsAtOrBelow += level.sellQuantity;
        const Candidate candidate{
            .price = level.price,
            .volume = std::min(buysAtOrAbove, sellsAtOrBelow),
            .imbalance = buysAtOrAbove - sellsAtOrBelow,
            .distance = std::abs(level.price.ticks() - reference.ticks()),
        };
        buysAtOrAbove -= level.buyQuantity;

        const auto rank = best ? compareRank(candidate, *best) : std::strong_ordering::less;
        if (rank < 0) {
            best = candidate;
            tied.reset();
        } else if (rank == 0) {
            assert(!tied);
            tied = candidate;
        }
    }

    if (!best || best->volume == 0) return AuctionResult{};
    if (!tied) return toResult(*best);

    const Side oldest = oldestEligibleSide(orders, best->price, tied->price);
    return toResult(oldest == Side::Buy ? *best : *tied);
}

}  // namespace auction
