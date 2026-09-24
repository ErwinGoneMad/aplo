# Closing Auction Calculator

A dependency-free C++20 command-line program that determines an equity closing-auction price from buy and sell orders.

## Build and run

Requirements:

- macOS or Linux
- CMake 3.20 or newer
- GCC or Clang with C++20 support

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/auction -i examples/input.txt -r 275.99
```

Output:

```text
Price: 270.39
Volume: 100
Imbalance: 100
```

The exit code is `0` on success, `1` for an input-data error, and `2` for invalid command-line usage.

## Input

The input is an ASCII CSV file without a header. Each line has five fields:

```text
timestamp,symbol,side,quantity,price
```

- `timestamp`: unsigned 64-bit nanoseconds since 1970-01-01
- `symbol`: one or more uppercase letters
- `side`: `B` for buy or `S` for sell
- `quantity`: positive integer
- `price`: non-negative decimal; zero denotes a market order

Exactly one symbol is accepted per file. Malformed input is rejected at the first error with the filename and line number.

## Auction semantics

1. Candidate prices are the distinct non-zero limit prices. Market orders do not introduce a candidate.
2. At candidate price `p`, eligible orders are market orders, buys priced at or above `p`, and sells priced at or below `p`.
3. Candidates are ranked by maximum crossed volume: `min(eligible buys, eligible sells)`.
4. Remaining candidates are ranked by minimum absolute imbalance. The reported value remains signed: `eligible buys - eligible sells`.
5. Remaining candidates are ranked by distance to the reference price.
6. If two prices remain, the oldest eligible buy selects the lower price and the oldest eligible sell selects the higher price.

Eligibility at the candidate price is inclusive. If there is no limit-price candidate, or the maximum crossed volume is zero, all three output values are zero.

### Final tie-break

Eligibility differs between two tied prices, so “oldest eligible order” needs a precise interpretation. The implementation considers the union of:

- market orders
- buys eligible at the lower price
- sells eligible at the higher price

This ensures that the deciding order is eligible at the price selected by its side. The smallest timestamp is oldest; file order breaks equal timestamps.

## Correctness, maintainability, and complexity

### Correctness

- Prices are exact fixed-point integers with eight decimal places; no binary floating-point comparison is used.
- Inputs that require rounding or exceed the price range are rejected.
- The total order quantity must fit in `INT64_MAX`, which bounds cumulative volumes and signed imbalance.
- Unit, end-to-end, sanitizer, and deterministic randomized differential tests cover normal and adversarial cases.

### Maintainability

- Price handling, parsing, auction logic, and command-line I/O are separate components.
- `runAuction` is deterministic and has no I/O or hidden state.
- The project uses standard C++20 and CMake with no third-party dependencies.
- Assumptions and limitations are documented rather than encoded implicitly.

### Complexity

Limit orders are sorted and merged by price, then evaluated in one ascending scan. A final linear pass is made only when the last tie-break is needed.

- Time: `O(n log n)`
- Additional memory: `O(n)`

## Tests

```sh
ctest --test-dir build --output-on-failure
```

The suite contains 40 unit and differential tests plus 16 end-to-end command-line tests. It covers:

- each auction ranking rule and the final tie-break
- market, empty, one-sided, and non-crossing books
- equal-price aggregation and timestamp ordering
- malformed input and command-line errors
- fixed-point and arithmetic boundaries
- 40,000 generated books checked against an independent brute-force implementation

For AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DAUCTION_SANITIZE=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

## Performance investigation

On an Apple M1 Pro, optimized in-memory parsing measured approximately 265 ns per order. Auction calculation measured approximately 50–84 ns per order, depending on the number of distinct price levels. Parsing dominated the combined workload; sorting dominated the auction phase on a wide price distribution. These measurements confirm the expected `O(n log n)` behavior and are not portable latency guarantees.

Two branches preserve the input-performance experiments:

- [`bench/parser-comparison`](https://github.com/ErwinGoneMad/aplo/tree/bench/parser-comparison) compares `std::getline`, whole-file buffering, and memory mapping. Median warm-cache ingestion of a 37 MB, one-million-order file was approximately 280 ms, 144 ms, and 133 ms respectively; unlike the in-memory figure above, these measurements include opening and acquiring the file.
- [`feature/mmap-parser`](https://github.com/ErwinGoneMad/aplo/tree/feature/mmap-parser) contains the resulting POSIX mmap implementation and documents its trade-offs.

## Known limitations and assumptions

- A file may contain only one symbol.
- Prices support up to eight fractional digits and a maximum value of `92233720368.54775807`.
- Quantities must be positive, and their total must fit in `INT64_MAX`.
- Orders and temporary price levels are held in memory.
- The final tie-break union and equal-timestamp file-order rule are explicit choices for otherwise ambiguous cases.
- A market-only book returns `0 / 0 / 0` because it has no limit-price candidate.
