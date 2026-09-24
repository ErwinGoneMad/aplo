#pragma once

#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "auction.hpp"
#include "order.hpp"
#include "price.hpp"

namespace auction {

inline std::ostream& operator<<(std::ostream& out, const AuctionResult& result) {
    return out << "{price " << result.price << ", volume " << result.volume << ", imbalance "
               << result.imbalance << "}";
}

}  // namespace auction

namespace testkit {

inline auction::Price px(std::string_view text) {
    const auto price = auction::parsePrice(text);
    if (!price) throw std::invalid_argument("bad test price " + std::string(text));
    return *price;
}

inline auction::Order buy(std::uint64_t timestamp, std::int64_t quantity, std::string_view price) {
    return {.timestamp = timestamp, .quantity = quantity, .price = px(price), .side = auction::Side::Buy};
}

inline auction::Order sell(std::uint64_t timestamp, std::int64_t quantity, std::string_view price) {
    return {.timestamp = timestamp, .quantity = quantity, .price = px(price), .side = auction::Side::Sell};
}

inline auction::AuctionResult result(std::string_view price, std::int64_t volume, std::int64_t imbalance) {
    return {.price = px(price), .volume = volume, .imbalance = imbalance};
}

}  // namespace testkit
