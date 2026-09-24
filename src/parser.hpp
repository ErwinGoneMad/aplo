#pragma once

#include <istream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "order.hpp"

namespace auction {

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct OrderFile {
    std::string symbol;         // empty when the input holds no orders
    std::vector<Order> orders;  // in input order
};

// Reads one order per line: "timestamp,SYMBOL,B|S,quantity,price".
// Throws ParseError naming `source` and the line number on the first malformed
// line, on a second symbol, or when the total quantity would exceed INT64_MAX
// (which keeps every auction sum and imbalance within int64).
OrderFile parseOrders(std::istream& in, std::string_view source);

}  // namespace auction
