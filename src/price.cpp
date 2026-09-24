#include "price.hpp"

#include <algorithm>
#include <limits>

namespace auction {

namespace {

bool isDigits(std::string_view text) {
    return std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

}  // namespace

std::optional<Price> parsePrice(std::string_view text) {
    const std::size_t dot = text.find('.');
    const std::string_view wholeText = text.substr(0, dot);
    const std::string_view fractionText =
        dot == std::string_view::npos ? std::string_view{} : text.substr(dot + 1);

    if (wholeText.empty() || !isDigits(wholeText)) return std::nullopt;
    if (dot != std::string_view::npos && (fractionText.empty() || !isDigits(fractionText))) {
        return std::nullopt;
    }
    if (fractionText.size() > Price::kDecimals) return std::nullopt;

    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    constexpr std::int64_t kMaxWhole = kMax / Price::kScale;

    std::int64_t whole = 0;
    for (const char c : wholeText) {
        whole = whole * 10 + (c - '0');
        if (whole > kMaxWhole) return std::nullopt;
    }

    std::int64_t fraction = 0;
    for (const char c : fractionText) fraction = fraction * 10 + (c - '0');
    for (std::size_t i = fractionText.size(); i < Price::kDecimals; ++i) fraction *= 10;

    if (whole > (kMax - fraction) / Price::kScale) return std::nullopt;
    return Price::fromTicks(whole * Price::kScale + fraction);
}

std::string formatPrice(Price price) {
    std::string text = std::to_string(price.ticks() / Price::kScale);
    const std::int64_t fraction = price.ticks() % Price::kScale;
    if (fraction == 0) return text;

    std::string digits = std::to_string(fraction);
    digits.insert(0, Price::kDecimals - digits.size(), '0');
    digits.erase(digits.find_last_not_of('0') + 1);
    text += '.';
    text += digits;
    return text;
}

}  // namespace auction
