# Closing Auction Calculator

A dependency-free C++20 command-line program that calculates an equity closing-auction price from a file of buy and sell orders.

It outputs:

- the selected auction price;
- the total crossed volume; and
- the remaining signed imbalance, positive for a buy surplus and negative for a sell surplus.

## Build and run

Requirements:

- CMake 3.20 or newer;
- a C++20 compiler.

```sh
cmake -S . -B build
cmake --build build -j
./build/auction -i examples/input.txt -r 275.99
```

Example output:

```text
Price: 270.39
Volume: 100
Imbalance: 100
```

The program returns `0` on success, `1` for an input-data error, and `2` for invalid command-line usage.

## Input format

The input is an ASCII CSV file without a header. Each non-empty line contains:

```text
timestamp,symbol,side,quantity,price
```

For example:

```text
1527604196773077003,AAPL,S,500,270.5700
1527604199695788161,AAPL,B,100,270.3900
1527604199397997988,AAPL,S,100,0
1527604199974781594,AAPL,S,900,278.00
1527604200211637272,AAPL,B,100,0
```

- `timestamp` is an unsigned 64-bit integer.
- `symbol` contains uppercase letters only. This implementation accepts exactly one symbol per file.
- `side` is `B` for buy or `S` for sell.
- `quantity` is a positive integer.
- `price` is a non-negative decimal. A price of zero denotes a market order.

Malformed input is rejected at the first error with the source file and line number.

## Auction rules

The implementation applies the assignment rules in this order:

1. Candidate prices are the distinct non-zero order prices. Market orders do not introduce a candidate price.
2. At a candidate price `p`, eligible orders are:
   - market orders;
   - buy limits priced at or above `p`; and
   - sell limits priced at or below `p`.
3. Select the candidate with the greatest executable volume: `min(eligible buys, eligible sells)`.
4. If several candidates remain, select the one with the smallest absolute imbalance. The reported imbalance remains signed: `eligible buys - eligible sells`.
5. If several candidates remain, select the one closest to the supplied reference price.
6. If two candidates remain, use the assignment's oldest-eligible-order rule: a buy selects the lower price and a sell selects the higher price.

Orders priced exactly at the candidate are included. This is consistent with the assignment's rule that buy and sell prices are compatible when the buy price is greater than or equal to the sell price.

If there is no non-market candidate price, or if the maximum executable volume is zero, the program outputs price `0`, volume `0`, and imbalance `0`.

### Final tie-break interpretation

The phrase “oldest eligible order” is ambiguous when eligibility differs between the two tied prices. This implementation considers the oldest order eligible at either tied price:

- market orders;
- buys eligible at the lower price; and
- sells eligible at the higher price.

This makes the deciding order eligible at the price selected by its side. Age is determined by the smallest timestamp. If timestamps are equal, earlier file order wins.

## Price and overflow handling

Prices use exact fixed-point integers with eight decimal places. They are parsed directly from text without passing through binary floating point, so equal price levels and reference-price distances are compared exactly.

Inputs with more than eight fractional digits, exponent notation, signs, whitespace, or a value outside the representable range are rejected rather than rounded.

Quantities and results use signed 64-bit integers internally. The parser requires the sum of all order quantities to fit in `INT64_MAX`. This guarantees that cumulative buy and sell volumes, crossed volume, and signed imbalance cannot overflow during the auction calculation.

## Algorithm and complexity

`runAuction` performs the following steps:

1. Separate market-order quantities from limit orders.
2. Sort limit orders by price and merge equal prices into levels.
3. Scan the levels in ascending order while maintaining eligible buy and sell totals.
4. Rank each candidate by volume, absolute imbalance, and distance to the reference price.
5. If required, make one additional linear pass to resolve the final tie.

The total complexity is `O(n log n)` time and `O(n)` additional memory. The auction calculation is deterministic and single-threaded.

## Correctness, maintainability, and algorithm complexity

These are the three main priorities of the assignment, and the implementation addresses each directly.

### Correctness

- Prices are parsed and compared as exact fixed-point values rather than binary floating point.
- The auction rules are applied in their stated order, with the ambiguous cases documented explicitly.
- Input is validated before calculation, and the total-quantity bound prevents arithmetic overflow.
- Unit, end-to-end, sanitizer, and randomized differential tests cover both normal and adversarial cases.

### Maintainability

- Price handling, input parsing, auction logic, and command-line concerns are separated into small components.
- `runAuction` is a deterministic function with no I/O or hidden state, which makes it straightforward to reason about and test.
- The project uses standard C++20 and CMake without third-party dependencies.
- Assumptions and limitations are recorded below instead of being left implicit in the implementation.

### Algorithm complexity

- Limit orders are sorted and merged once, then evaluated in a single ascending scan.
- The production algorithm runs in `O(n log n)` time and uses `O(n)` additional memory.
- The intentionally slower brute-force implementation is used only as a test oracle and is not part of the production path.

## Tests

Build and run the complete test suite with:

```sh
ctest --test-dir build --output-on-failure
```

The project contains 40 unit and differential tests plus 16 command-line test entries. Coverage includes:

- every auction ranking rule;
- market, one-sided, empty, and non-crossing books;
- equal price aggregation;
- timestamp and final tie behavior;
- parser and command-line rejection paths;
- values near the arithmetic limit; and
- 40,000 deterministic randomized comparisons against a separate brute-force implementation.

To run with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DAUCTION_SANITIZE=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

An optional synthetic benchmark is built as `auction_bench`:

```sh
./build/auction_bench [orders] [price_levels] [repetitions]
```

## Performance investigation

An optimized Apple M1 Pro profile measured parsing at approximately 265 ns per order and the auction calculation at approximately 50–84 ns per order, depending on the number of distinct price levels. Parsing dominated total CPU time, while sorting dominated the auction phase on a deliberately wide price distribution. The measurements confirmed the documented `O(n log n)` behavior and found no auction-algorithm performance defect.

Separate experimental branches compared the current `std::getline` parser with whole-file buffered and memory-mapped input. On a warm 37 MB, one-million-order file, their median ingestion times were approximately 280 ms, 144 ms, and 133 ms respectively. Those local figures motivated an optional mmap implementation without complicating this main assignment branch; they are reproducible development measurements, not portable latency guarantees.

## Project layout

- `src/price.*`: exact price parsing and formatting.
- `src/parser.*`: input validation and order-file parsing.
- `src/auction.*`: auction calculation.
- `src/main.cpp`: command-line handling and output.
- `tests/`: unit, differential, and end-to-end tests.
- `testdata/`: assignment and adversarial input cases with expected output.
- `bench/bench.cpp`: synthetic parser and auction benchmark.

## Known limitations and assumptions

- Exactly one symbol is accepted per input file.
- Prices are limited to eight fractional digits and approximately `9.22e10` in magnitude. The assignment does not state a maximum precision or range.
- Quantities must be positive and their total must fit in `INT64_MAX`.
- All parsed orders and a temporary vector of price levels are held in memory.
- The final tie-break interpretation and equal-timestamp file-order rule are explicit implementation assumptions because the assignment does not define those cases fully.
- A market-only book returns `0 / 0 / 0` because the assignment requires the auction price to be a non-market order price.
