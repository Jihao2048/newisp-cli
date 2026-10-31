# Unit tests.
#
# These cover the pieces that are pure logic: the Intel HEX parser, the chip
# table lookup, the family predicates and the packet framing/checksum code. The
# protocol exchanges that need a live link are covered end-to-end by
# tests/pty_test.py instead, because a fake socket cannot stand in for a tty's
# timing.
#
# No test framework is used: the cases are a handful of assertions, and pulling
# in GoogleTest or Catch2 would make the build depend on fetching something for
# no real gain. Each test file is a small program that returns non-zero on
# failure, which is all CTest needs.

add_executable(newisp_tests
    test_hex_file.cpp
    test_chip_table.cpp
    test_chip_logic.cpp
    test_protocol_framing.cpp
    test_i18n.cpp
    test_options.cpp
    test_main.cpp
)

target_link_libraries(newisp_tests PRIVATE newisp_transport)
# test_options.cpp calls ParseCommandLine, which lives in the CLI target rather
# than the transport library.
target_sources(newisp_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/cli/options.cpp)

add_test(NAME newisp_tests COMMAND newisp_tests)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# The CLI tests read the tool's output with text=True, which decodes using the
# locale. On a Chinese Windows that is code page 936, and the log lines are
# UTF-8, so every read fails with a UnicodeDecodeError before any assertion
# runs. PYTHONUTF8 makes the child decode as UTF-8 regardless of the console.
set_tests_properties(newisp_tests PROPERTIES
    ENVIRONMENT "PYTHONUTF8=1")

add_test(NAME newisp_cli_preflight COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/cli_preflight_test.py $<TARGET_FILE:newisp>)
set_tests_properties(newisp_cli_preflight PROPERTIES
    ENVIRONMENT "PYTHONUTF8=1")
if(UNIX)
    add_test(NAME newisp_pty_preflight COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/pty_preflight_test.py $<TARGET_FILE:newisp>)
    set_tests_properties(newisp_pty_preflight PROPERTIES
        ENVIRONMENT "PYTHONUTF8=1")
endif()
