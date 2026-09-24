#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "auction.hpp"
#include "input_file.hpp"
#include "parser.hpp"
#include "price.hpp"

namespace {

constexpr int kExitSuccess = 0;
constexpr int kExitDataError = 1;
constexpr int kExitUsageError = 2;

class UsageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Options {
    std::string inputPath;
    auction::Price reference;
};

void printUsage(std::ostream& out, std::string_view program) {
    out << "Usage: " << program << " -i <input_file> -r <reference_price>\n";
}

bool isHelpFlag(std::string_view arg) { return arg == "-h" || arg == "--help"; }

Options parseArguments(int argc, char** argv) {
    std::optional<std::string> inputPath;
    std::optional<auction::Price> reference;

    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (isHelpFlag(flag)) throw UsageError(flag + " must be used alone");
        if (flag != "-i" && flag != "-r") throw UsageError("unknown argument '" + flag + "'");
        if (i + 1 >= argc) throw UsageError("missing value for " + flag);
        const std::string_view value = argv[++i];

        if (flag == "-i") {
            if (inputPath) throw UsageError("-i given more than once");
            inputPath = std::string(value);
        } else {
            if (reference) throw UsageError("-r given more than once");
            reference = auction::parsePrice(value);
            if (!reference) {
                throw UsageError("invalid reference price '" + std::string(value) +
                                 "' (non-negative decimal, at most 8 decimals)");
            }
        }
    }

    if (!inputPath) throw UsageError("missing -i <input_file>");
    if (!reference) throw UsageError("missing -r <reference_price>");
    return Options{.inputPath = *inputPath, .reference = *reference};
}

}  // namespace

int main(int argc, char** argv) {
    const std::string_view program = argc > 0 ? argv[0] : "auction";

    if (argc == 2 && isHelpFlag(argv[1])) {
        printUsage(std::cout, program);
        return kExitSuccess;
    }

    Options options;
    try {
        options = parseArguments(argc, argv);
    } catch (const UsageError& error) {
        std::cerr << "error: " << error.what() << '\n';
        printUsage(std::cerr, program);
        return kExitUsageError;
    }

    try {
        const auction::InputFile input{options.inputPath};
        const auction::OrderFile file = auction::parseOrders(input.bytes(), options.inputPath);
        const auction::AuctionResult result = auction::runAuction(file.orders, options.reference);
        std::cout << "Price: " << result.price << '\n'
                  << "Volume: " << result.volume << '\n'
                  << "Imbalance: " << result.imbalance << '\n';
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return kExitDataError;
    }
    return kExitSuccess;
}
