#include "parser.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>

namespace auction {

namespace {

constexpr std::size_t kFieldCount = 5;
constexpr std::int64_t kMaxQuantity = std::numeric_limits<std::int64_t>::max();

using Fields = std::array<std::string_view, kFieldCount>;

std::optional<Fields> splitFields(std::string_view line) {
    Fields fields;
    std::size_t count = 0;
    std::size_t start = 0;
    while (true) {
        if (count == kFieldCount) return std::nullopt;
        const std::size_t comma = line.find(',', start);
        fields[count++] = line.substr(start, comma - start);
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    if (count != kFieldCount) return std::nullopt;
    return fields;
}

std::optional<std::uint64_t> parseUnsigned(std::string_view text) {
    std::uint64_t value = 0;
    const char* const last = text.data() + text.size();
    const auto [end, error] = std::from_chars(text.data(), last, value);
    if (text.empty() || error != std::errc{} || end != last) return std::nullopt;
    return value;
}

bool isSymbol(std::string_view text) {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](char c) { return c >= 'A' && c <= 'Z'; });
}

std::string quoted(std::string_view text) { return "'" + std::string(text) + "'"; }

}  // namespace

OrderFile parseOrders(std::istream& in, std::string_view source) {
    OrderFile file;
    std::int64_t totalQuantity = 0;
    std::string line;

    for (std::size_t lineNumber = 1; std::getline(in, line); ++lineNumber) {
        const auto fail = [&](const std::string& reason) {
            throw ParseError(std::string(source) + ":" + std::to_string(lineNumber) + ": " + reason);
        };

        std::string_view text = line;
        if (!text.empty() && text.back() == '\r') text.remove_suffix(1);

        const std::optional<Fields> fields = splitFields(text);
        if (!fields) fail("expected 5 comma-separated fields (timestamp,symbol,side,quantity,price)");
        const auto& [timestampText, symbol, sideText, quantityText, priceText] = *fields;

        const std::optional<std::uint64_t> timestamp = parseUnsigned(timestampText);
        if (!timestamp) fail("invalid timestamp " + quoted(timestampText));

        if (!isSymbol(symbol)) fail("invalid symbol " + quoted(symbol) + " (uppercase letters only)");
        if (file.symbol.empty()) {
            file.symbol = symbol;
        } else if (symbol != file.symbol) {
            fail("symbol " + quoted(symbol) + " differs from " + quoted(file.symbol) +
                 "; one auction runs one symbol");
        }

        if (sideText != "B" && sideText != "S") fail("invalid side " + quoted(sideText) + " (B or S)");

        const std::optional<std::uint64_t> quantity = parseUnsigned(quantityText);
        if (!quantity || *quantity == 0 || *quantity > static_cast<std::uint64_t>(kMaxQuantity)) {
            fail("invalid quantity " + quoted(quantityText) + " (positive integer)");
        }
        const auto signedQuantity = static_cast<std::int64_t>(*quantity);
        if (signedQuantity > kMaxQuantity - totalQuantity) {
            fail("total quantity exceeds " + std::to_string(kMaxQuantity));
        }
        totalQuantity += signedQuantity;

        const std::optional<Price> price = parsePrice(priceText);
        if (!price) {
            fail("invalid price " + quoted(priceText) + " (non-negative decimal, at most " +
                 std::to_string(Price::kDecimals) + " decimals)");
        }

        file.orders.push_back(Order{
            .timestamp = *timestamp,
            .quantity = signedQuantity,
            .price = *price,
            .side = sideText == "B" ? Side::Buy : Side::Sell,
        });
    }

    if (in.bad()) throw ParseError(std::string(source) + ": read error");
    return file;
}

}  // namespace auction
