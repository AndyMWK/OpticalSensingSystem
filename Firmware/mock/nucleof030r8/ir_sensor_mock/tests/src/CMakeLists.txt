# ---------------------------------------------------------------------------
# One executable per test file.
#
# Unity has no runtime test discovery: each test file compiles into its own
# small program with its own main(). That is a feature here - each executable
# is a fresh process, so the file-scope `static` state inside fsm.c and
# ir_processing.c cannot leak from one test FILE into another. (It still
# persists between tests within a single file; the test files deal with that
# explicitly.)
#
# To add a new test: drop test_<something>.c in this directory and add its
# name to the list below.
# ---------------------------------------------------------------------------

set(TEST_SOURCES
    test_fifo.c
    test_ir_processor.c
    test_fsm.c
)

foreach(test_source IN LISTS TEST_SOURCES)
    # test_fifo.c -> test_fifo
    get_filename_component(test_name ${test_source} NAME_WE)

    add_executable(${test_name} ${test_source})

    # fw_under_test carries the include paths, the HAL stub flag and libm
    # along with it, because those were declared PUBLIC.
    target_link_libraries(${test_name} PRIVATE fw_under_test unity)

    if(MSVC)
        target_compile_options(${test_name} PRIVATE /W3)
    else()
        target_compile_options(${test_name} PRIVATE -Wall -Wextra)
    endif()

    # Registers the executable with CTest under its own name, so
    # `ctest -R fifo` runs just the FIFO tests. A test passes if the process
    # exits with code 0, which is exactly what UNITY_END() returns when no
    # assertion failed.
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
