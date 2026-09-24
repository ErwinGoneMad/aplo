# Runs PROGRAM with ARGS ('|'-separated), then checks the exit code and,
# when EXPECTED_STDOUT is set, that stdout matches that file byte for byte.
string(REPLACE "|" ";" arg_list "${ARGS}")
execute_process(
    COMMAND ${PROGRAM} ${arg_list}
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE actual_stdout
    ERROR_VARIABLE actual_stderr)

if(NOT exit_code EQUAL EXPECTED_EXIT)
    message(FATAL_ERROR
        "expected exit code ${EXPECTED_EXIT}, got ${exit_code}\nstdout:\n${actual_stdout}\nstderr:\n${actual_stderr}")
endif()

if(DEFINED EXPECTED_STDOUT)
    file(READ ${EXPECTED_STDOUT} expected_stdout)
    if(NOT actual_stdout STREQUAL expected_stdout)
        message(FATAL_ERROR "stdout mismatch\nexpected:\n${expected_stdout}\nactual:\n${actual_stdout}")
    endif()
endif()
