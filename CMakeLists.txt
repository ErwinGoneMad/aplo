cmake_minimum_required(VERSION 3.20)
project(closing_auction LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(AUCTION_BUILD_BENCH "Build the auction_bench benchmark" ON)
option(AUCTION_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(auction_warnings INTERFACE)
target_compile_options(auction_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror)

if(AUCTION_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(auction_core STATIC
    src/price.cpp
    src/parser.cpp
    src/input_file.cpp
    src/auction.cpp)
target_include_directories(auction_core PUBLIC src)
target_link_libraries(auction_core PRIVATE auction_warnings)

add_executable(auction src/main.cpp)
target_link_libraries(auction PRIVATE auction_core auction_warnings)

enable_testing()

add_executable(auction_tests
    tests/test_main.cpp
    tests/brute_force.cpp
    tests/test_price.cpp
    tests/test_parser.cpp
    tests/test_auction.cpp
    tests/test_differential.cpp)
target_link_libraries(auction_tests PRIVATE auction_core auction_warnings)
add_test(NAME unit_tests COMMAND auction_tests)

# End-to-end checks of the command-line program. Arguments are separated by '|'.
function(add_cli_test name args expected_exit)
    set(extra "")
    if(ARGC GREATER 3)
        set(extra "-DEXPECTED_STDOUT=${ARGV3}")
    endif()
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:auction>
            "-DARGS=${args}"
            -DEXPECTED_EXIT=${expected_exit}
            ${extra}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.cmake)
endfunction()

set(EXAMPLES ${CMAKE_CURRENT_SOURCE_DIR}/examples)
set(TESTDATA ${CMAKE_CURRENT_SOURCE_DIR}/testdata)
add_cli_test(cli_spec_example "-i|${EXAMPLES}/input.txt|-r|275.99" 0 ${EXAMPLES}/output.txt)
add_cli_test(cli_assignment_example "-i|${TESTDATA}/01_assignment_example.csv|-r|275.99" 0 ${TESTDATA}/01_assignment_example.out)
add_cli_test(cli_liquid_arkw "-i|${TESTDATA}/02_liquid_arkw.csv|-r|52.90" 0 ${TESTDATA}/02_liquid_arkw.out)
add_cli_test(cli_ranking_rules "-i|${TESTDATA}/03_ranking_rules.csv|-r|273.50" 0 ${TESTDATA}/03_ranking_rules.out)
add_cli_test(cli_oldest_tiebreak "-i|${TESTDATA}/04_oldest_order_tiebreak.csv|-r|271" 0 ${TESTDATA}/04_oldest_order_tiebreak.out)
add_cli_test(cli_edge_no_cross "-i|${TESTDATA}/05_edge_no_cross.csv|-r|43.50" 0 ${TESTDATA}/05_edge_no_cross.out)
add_cli_test(cli_edge_market_only "-i|${TESTDATA}/06_edge_market_only.csv|-r|53.25" 0 ${TESTDATA}/06_edge_market_only.out)
add_cli_test(cli_missing_reference "-i|${EXAMPLES}/input.txt" 2)
add_cli_test(cli_bad_reference "-i|${EXAMPLES}/input.txt|-r|abc" 2)
add_cli_test(cli_unknown_flag "-x|1" 2)
add_cli_test(cli_missing_file "-i|${EXAMPLES}/does_not_exist.txt|-r|1" 1)
add_cli_test(cli_malformed_input "-i|${EXAMPLES}/malformed.txt|-r|1" 1)
if(UNIX)
    add_cli_test(cli_non_regular_input "-i|${EXAMPLES}|-r|1" 1)
endif()

# The usage line echoes argv[0], which is the absolute path of the built program.
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/expected_help.txt
    CONTENT "Usage: $<TARGET_FILE:auction> -i <input_file> -r <reference_price>\n")
set(EXPECTED_HELP ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/expected_help.txt)
add_cli_test(cli_help_long "--help" 0 ${EXPECTED_HELP})
add_cli_test(cli_help_short "-h" 0 ${EXPECTED_HELP})
add_cli_test(cli_help_with_extra_argument "--help|garbage" 2)
# "--help" here is the value of -i, i.e. a file path that does not exist.
add_cli_test(cli_help_as_input_value "-i|--help|-r|1" 1)

if(AUCTION_BUILD_BENCH)
    add_executable(auction_bench bench/bench.cpp)
    target_link_libraries(auction_bench PRIVATE auction_core auction_warnings)
endif()
