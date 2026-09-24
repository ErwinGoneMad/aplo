#pragma once

#include <cstdint>
#include <span>

#include "order.hpp"
#include "price.hpp"

namespace auction {

struct AuctionResult {
    Price price;                 // zero when no cross price exists
    std::int64_t volume = 0;     // shares matched, never negative
    std::int64_t imbalance = 0;  // eligible buy minus eligible sell volume at `price`

    friend bool operator==(const AuctionResult&, const AuctionResult&) = default;
};

// Finds the cross price: maximum matched volume, then minimum absolute
// imbalance, then closest to `reference`, then the lowest price if the oldest
// eligible order is a buy or the highest if it is a sell.
//
// Preconditions (established by parseOrders): quantities are positive and
// their total fits in int64. `orders` must be in input order, because input
// order breaks ties between equal timestamps.
//
// O(n log n) time, O(n) extra memory.
AuctionResult runAuction(std::span<const Order> orders, Price reference);

}  // namespace auction
