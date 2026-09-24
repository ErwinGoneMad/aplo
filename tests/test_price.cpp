#include <cstdint>
#include <optional>
#include <string_view>

#include "price.hpp"
#include "test_framework.hpp"

using auction::formatPrice;
using auction::parsePrice;
using auction::Price;

TEST(PriceParsesDecimalsExactly) {
    CHECK_EQ(parsePrice("270.57")->ticks(), 27'057'000'000);
    CHECK_EQ(parsePrice("270.5700")->ticks(), 27'057'000'000);
    CHECK_EQ(parsePrice("0.00000001")->ticks(), 1);
    CHECK_EQ(parsePrice("007.50")->ticks(), 750'000'000);
    CHECK_EQ(parsePrice("278")->ticks(), 27'800'000'000);
}

TEST(PriceZeroIsMarket) {
    CHECK(parsePrice("0")->isMarket());
    CHECK(parsePrice("0.0000")->isMarket());
    CHECK(!parsePrice("0.00000001")->isMarket());
}

TEST(PriceAcceptsLargestRepresentableValue) {
    CHECK_EQ(parsePrice("92233720368.54775807")->ticks(), INT64_MAX);
}

TEST(PriceRejectsMalformedText) {
    for (const std::string_view text :
         {"", ".", "1.", ".5", "+1", "-1", "1e2", "1,5", " 1", "1 ", "nan", "inf", "1..2", "1.2.3",
          "0x10", "1.123456789", "92233720368.54775808", "99999999999999999999"}) {
        if (parsePrice(text).has_value()) {
            CHECK_EQ(std::string_view("accepted"), text);
        }
    }
}

TEST(PriceFormatsShortestExactDecimal) {
    CHECK_EQ(formatPrice(Price{}), "0");
    CHECK_EQ(formatPrice(*parsePrice("270.3900")), "270.39");
    CHECK_EQ(formatPrice(*parsePrice("278.00")), "278");
    CHECK_EQ(formatPrice(*parsePrice("0.00000001")), "0.00000001");
    CHECK_EQ(formatPrice(*parsePrice("12.05")), "12.05");
}

TEST(PriceRoundTripsThroughText) {
    for (const std::string_view text : {"1", "0.1", "99.99", "123456.00000001", "92233720368.54775807"}) {
        CHECK_EQ(formatPrice(*parsePrice(text)), text);
    }
}

TEST(PriceOrdersNumerically) {
    CHECK(*parsePrice("9.99") < *parsePrice("10"));
    CHECK(*parsePrice("10.0") == *parsePrice("10"));
}
