#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

#include "parser.hpp"
#include "test_framework.hpp"
#include "test_helpers.hpp"

using auction::OrderFile;
using auction::ParseError;
using auction::Side;
using testkit::px;

namespace {

OrderFile parse(const std::string& text) {
    std::istringstream in(text);
    return auction::parseOrders(in, "input.txt");
}

}  // namespace

TEST(ParserReadsSpecExample) {
    const OrderFile file = parse(
        "1527604196773077003,AAPL,S,500,270.5700\n"
        "1527604199695788161,AAPL,B,100,270.3900\n"
        "1527604199397997988,AAPL,S,100,0\n");

    CHECK_EQ(file.symbol, "AAPL");
    CHECK_EQ(file.orders.size(), 3u);
    CHECK_EQ(file.orders[0].timestamp, 1527604196773077003u);
    CHECK(file.orders[0].side == Side::Sell);
    CHECK_EQ(file.orders[0].quantity, 500);
    CHECK_EQ(file.orders[0].price, px("270.57"));
    CHECK(file.orders[1].side == Side::Buy);
    CHECK(file.orders[2].price.isMarket());
}

TEST(ParserKeepsInputOrder) {
    const OrderFile file = parse("30,X,B,1,1\n10,X,S,1,1\n20,X,B,1,0\n");
    CHECK_EQ(file.orders.size(), 3u);
    CHECK_EQ(file.orders[0].timestamp, 30u);
    CHECK_EQ(file.orders[1].timestamp, 10u);
    CHECK_EQ(file.orders[2].timestamp, 20u);
}

TEST(ParserAcceptsCrlfAndMissingFinalNewline) {
    const OrderFile file = parse("1,AAPL,B,100,10.5\r\n2,AAPL,S,50,10");
    CHECK_EQ(file.orders.size(), 2u);
    CHECK_EQ(file.orders[0].price, px("10.5"));
    CHECK_EQ(file.orders[1].quantity, 50);
}

TEST(ParserAcceptsEmptyInput) {
    const OrderFile file = parse("");
    CHECK(file.symbol.empty());
    CHECK(file.orders.empty());
}

TEST(ParserRejectsMalformedLines) {
    struct Case {
        std::string_view line;
        std::string_view messageFragment;
    };
    for (const Case& c : {
             Case{"", "expected 5 comma-separated fields"},
             Case{"1,AAPL,B,100", "expected 5 comma-separated fields"},
             Case{"1,AAPL,B,100,1,2", "expected 5 comma-separated fields"},
             Case{"x,AAPL,B,100,1", "invalid timestamp 'x'"},
             Case{"-1,AAPL,B,100,1", "invalid timestamp"},
             Case{" 1,AAPL,B,100,1", "invalid timestamp"},
             Case{"18446744073709551616,AAPL,B,100,1", "invalid timestamp"},
             Case{"1,aapl,B,100,1", "invalid symbol 'aapl'"},
             Case{"1,,B,100,1", "invalid symbol ''"},
             Case{"1,AAPL1,B,100,1", "invalid symbol"},
             Case{"1,AAPL,X,100,1", "invalid side 'X'"},
             Case{"1,AAPL,b,100,1", "invalid side"},
             Case{"1,AAPL,BS,100,1", "invalid side"},
             Case{"1,AAPL,B,0,1", "invalid quantity '0'"},
             Case{"1,AAPL,B,-5,1", "invalid quantity"},
             Case{"1,AAPL,B,1.5,1", "invalid quantity"},
             Case{"1,AAPL,B,9223372036854775808,1", "invalid quantity"},
             Case{"1,AAPL,B,100,-1", "invalid price '-1'"},
             Case{"1,AAPL,B,100,1e3", "invalid price"},
             Case{"1,AAPL,B,100,abc", "invalid price"},
             Case{"1,AAPL,B,100, 1", "invalid price"},
             Case{"1,AAPL,B,100,", "invalid price"},
             Case{"1,AAPL,B,100,1.123456789", "invalid price"},
         }) {
        CHECK_THROWS_WITH(parse(std::string(c.line) + "\n"), ParseError, std::string(c.messageFragment));
    }
}

TEST(ParserReportsSourceAndLineNumber) {
    CHECK_THROWS_WITH(parse("1,AAPL,B,100,1\n2,AAPL,S,100,1\n3,AAPL,Q,100,1\n"), ParseError,
                      "input.txt:3: invalid side 'Q'");
}

TEST(ParserRejectsBlankLineInMiddle) {
    CHECK_THROWS_WITH(parse("1,AAPL,B,100,1\n\n2,AAPL,S,100,1\n"), ParseError, "input.txt:2:");
}

TEST(ParserRejectsSecondSymbol) {
    CHECK_THROWS_WITH(parse("1,AAPL,B,100,1\n2,MSFT,S,100,1\n"), ParseError,
                      "input.txt:2: symbol 'MSFT' differs from 'AAPL'");
}

TEST(ParserRejectsTotalQuantityOverflow) {
    const std::string half = std::to_string(INT64_MAX / 2 + 1);
    CHECK_THROWS_WITH(parse("1,A,B," + half + ",1\n2,A,S," + half + ",1\n"), ParseError,
                      "input.txt:2: total quantity exceeds");
}

TEST(ParserAcceptsTotalQuantityAtLimit) {
    const std::string first = std::to_string(INT64_MAX - 1);
    const OrderFile file = parse("1,A,B," + first + ",1\n2,A,S,1,1\n");
    CHECK_EQ(file.orders.size(), 2u);
}
