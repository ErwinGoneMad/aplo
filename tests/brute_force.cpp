#include "brute_force.hpp"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace testkit {

using auction::AuctionResult;
using auction::Order;
using auction::Price;
using auction::Side;

namespace {

bool isEligible(const Order& order, Price crossPrice) {
    if (order.price.isMarket()) return true;
    return order.side == Side::Buy ? order.price >= crossPrice : order.price <= crossPrice;
}

struct Stats {
    Price price;
    std::int64_t buyVolume = 0;
    std::int64_t sellVolume = 0;

    std::int64_t volume() const { return std::min(buyVolume, sellVolume); }
    std::int64_t imbalance() const { return buyVolume - sellVolume; }
};

template <typename Key>
void keepMinimal(std::vector<Stats>& candidates, Key key) {
    const auto best = std::min_element(candidates.begin(), candidates.end(),
                                       [&](const Stats& a, const Stats& b) { return key(a) < key(b); });
    const auto bestKey = key(*best);
    std::erase_if(candidates, [&](const Stats& s) { return key(s) != bestKey; });
}

}  // namespace

BruteForceOutcome bruteForceAuction(std::span<const Order> orders, Price reference) {
    std::vector<Stats> candidates;
    for (const Order& order : orders) {
        if (order.price.isMarket()) continue;
        const bool seen = std::any_of(candidates.begin(), candidates.end(),
                                      [&](const Stats& s) { return s.price == order.price; });
        if (!seen) candidates.push_back(Stats{.price = order.price});
    }

    for (Stats& stats : candidates) {
        for (const Order& order : orders) {
            if (!isEligible(order, stats.price)) continue;
            (order.side == Side::Buy ? stats.buyVolume : stats.sellVolume) += order.quantity;
        }
    }

    if (candidates.empty()) return {};
    keepMinimal(candidates, [](const Stats& s) { return -s.volume(); });
    if (candidates.front().volume() == 0) return {};
    keepMinimal(candidates, [](const Stats& s) { return std::abs(s.imbalance()); });
    keepMinimal(candidates, [&](const Stats& s) { return std::abs(s.price.ticks() - reference.ticks()); });

    const auto toResult = [](const Stats& s) {
        return AuctionResult{.price = s.price, .volume = s.volume(), .imbalance = s.imbalance()};
    };
    if (candidates.size() == 1) return {.result = toResult(candidates.front())};

    const Order* oldest = nullptr;
    for (const Order& order : orders) {
        const bool eligibleSomewhere = std::any_of(candidates.begin(), candidates.end(),
                                                   [&](const Stats& s) { return isEligible(order, s.price); });
        if (eligibleSomewhere && (oldest == nullptr || order.timestamp < oldest->timestamp)) oldest = &order;
    }

    const auto byPrice = [](const Stats& a, const Stats& b) { return a.price < b.price; };
    const Stats& chosen = oldest->side == Side::Buy
                              ? *std::min_element(candidates.begin(), candidates.end(), byPrice)
                              : *std::max_element(candidates.begin(), candidates.end(), byPrice);
    return {.result = toResult(chosen), .usedFinalTieBreak = true};
}

}  // namespace testkit
