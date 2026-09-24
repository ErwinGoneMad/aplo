#pragma once

#include <cstdint>

#include "price.hpp"

namespace auction {

enum class Side : std::uint8_t { Buy, Sell };

struct Order {
    std::uint64_t timestamp = 0;  // nanoseconds since 1970-01-01
    std::int64_t quantity = 0;    // always > 0
    Price price;                  // zero means a market order
    Side side = Side::Buy;
};

}  // namespace auction
