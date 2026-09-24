#pragma once

#include <span>

#include "auction.hpp"

namespace testkit {

struct BruteForceOutcome {
    auction::AuctionResult result;
    bool usedFinalTieBreak = false;
};

// Literal O(n^2) transcription of the assignment's rules, written without
// the level aggregation or the two-candidate shortcut used by runAuction.
BruteForceOutcome bruteForceAuction(std::span<const auction::Order> orders, auction::Price reference);

}  // namespace testkit
