#pragma once

#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace auction {

// Exact, non-negative decimal price stored as an integer count of 1e-8 units.
// Zero denotes a market order.
class Price {
public:
    static constexpr std::size_t kDecimals = 8;
    static constexpr std::int64_t kScale = 100'000'000;

    constexpr Price() = default;

    static constexpr Price fromTicks(std::int64_t ticks) {
        assert(ticks >= 0);
        return Price{ticks};
    }

    constexpr std::int64_t ticks() const { return ticks_; }
    constexpr bool isMarket() const { return ticks_ == 0; }

    friend constexpr auto operator<=>(const Price&, const Price&) = default;

private:
    constexpr explicit Price(std::int64_t ticks) : ticks_{ticks} {}

    std::int64_t ticks_ = 0;
};

// Accepts digits with an optional '.' followed by 1 to kDecimals digits,
// e.g. "270.57" or "0". Signs, exponents, whitespace and values that would
// need rounding are rejected.
std::optional<Price> parsePrice(std::string_view text);

// Shortest exact decimal form: "270.39", "278", "0".
std::string formatPrice(Price price);

inline std::ostream& operator<<(std::ostream& out, Price price) {
    return out << formatPrice(price);
}

}  // namespace auction
