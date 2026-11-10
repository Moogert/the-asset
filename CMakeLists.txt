cmake_minimum_required(VERSION 3.16)
project(bench_ev_harness C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (MSVC)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
    # CMake's Windows/Clang platform maps its old-language option to c++14.
    # The control library needs the actual C++03 parser, without the C++ STL.
    set(CMAKE_CXX98_STANDARD_COMPILE_OPTION "-std=c++03")
endif()

add_library(capstone_control
    src/control/dual_supervisor.cpp
    src/control/control_codec.cpp
    src/control/control_runtime.cpp
    src/control/release_monitor.cpp
    src/control/release_runtime.cpp
)
target_include_directories(capstone_control PUBLIC include)
set_target_properties(capstone_control PROPERTIES
    CXX_STANDARD 98 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if (MSVC)
    # MSVC has no C++03 mode. The independent Clang build verifies that syntax.
    target_compile_options(capstone_control PRIVATE /W4 /WX /GR- /EHs-c- /fp:strict)
else()
    target_compile_options(capstone_control PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
        -fno-exceptions -fno-rtti -fno-fast-math)
endif()

add_executable(dual_control_demo src/app/dual_control_demo.cpp)
target_link_libraries(dual_control_demo PRIVATE capstone_control)
add_executable(controller_revision_demo src/app/controller_revision_demo.cpp)
target_link_libraries(controller_revision_demo PRIVATE capstone_control)

option(BUILD_FUZZERS "Build libFuzzer target when using Clang" OFF)

add_library(harness_core
    src/core/can_bus_model.c
    src/core/leaf_bms_adapter.c
    src/core/prius_inverter_adapter.c
    src/core/dual_drive.c
    src/core/charger_adapter.c
    src/core/safety_supervisor.c
    src/core/harness.c
    src/core/json_util.c
)
target_include_directories(harness_core PUBLIC include)
if (MSVC)
    target_compile_options(harness_core PRIVATE /W4 /wd4100)
else()
    target_compile_options(harness_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if (NOT WIN32)
        target_link_libraries(harness_core PUBLIC m)
    endif()
endif()

add_executable(bench_ev_harness
    src/app/main.c
    src/server/http_server.c
)
target_link_libraries(bench_ev_harness PRIVATE harness_core)
if (WIN32)
    target_link_libraries(bench_ev_harness PRIVATE ws2_32)
endif()

include(CTest)
if (BUILD_TESTING)
    add_library(civic_io
        firmware/lib/CapstoneIO/src/engine.cpp
        firmware/lib/CapstoneIO/src/emulator_io.cpp
        firmware/lib/CapstoneIO/src/diagnostics.cpp
        firmware/lib/CapstoneIO/src/transport.cpp
        firmware/lib/CapstoneIO/src/journal.cpp
        firmware/lib/CapstoneIO/src/indication.cpp
        firmware/lib/CapstoneIO/src/detail.cpp)
    target_include_directories(civic_io PUBLIC firmware/lib/CapstoneIO/src)
    set_target_properties(civic_io PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    add_executable(civic_io_tests tests/test_civic_io.cpp)
    target_include_directories(civic_io_tests PRIVATE include)
    target_link_libraries(civic_io_tests PRIVATE civic_io)
    set_target_properties(civic_io_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    add_test(NAME civic_io_regression COMMAND civic_io_tests)
    add_executable(civic_io_demo src/app/civic_io_demo.cpp)
    target_link_libraries(civic_io_demo PRIVATE civic_io)
    set_target_properties(civic_io_demo PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    add_executable(harness_tests tests/test_harness.c)
    target_link_libraries(harness_tests PRIVATE harness_core)
    add_test(NAME harness_regression COMMAND harness_tests)
    add_executable(dual_drive_tests tests/test_dual_drive.c)
    target_link_libraries(dual_drive_tests PRIVATE harness_core)
    add_test(NAME dual_drive_regression COMMAND dual_drive_tests)
    add_executable(cpp_control_tests tests/test_cpp_control.cpp)
    set_target_properties(cpp_control_tests PROPERTIES
        CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_link_libraries(cpp_control_tests PRIVATE capstone_control harness_core)
    add_test(NAME cpp_control_regression COMMAND cpp_control_tests)
    add_executable(controller_revision_tests tests/test_controller_revision.cpp)
    target_link_libraries(controller_revision_tests PRIVATE capstone_control civic_io)
    set_target_properties(controller_revision_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    add_test(NAME controller_revision_regression COMMAND controller_revision_tests)
    add_executable(emulator_io_tests tests/test_emulator_io.cpp)
    target_include_directories(emulator_io_tests PRIVATE firmware/include)
    target_link_libraries(emulator_io_tests PRIVATE civic_io)
    set_target_properties(emulator_io_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    add_test(NAME emulator_interface_regression COMMAND emulator_io_tests)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if (Python3_Interpreter_FOUND)
        add_test(NAME emulator_connector_exports
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/export_emulator_harness.py --check)
        add_test(NAME drivetrain_analysis_regression
            COMMAND ${Python3_EXECUTABLE} -m unittest discover -s tests -p test_analysis.py -v)
        set_tests_properties(drivetrain_analysis_regression PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME loopback_http_regression
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_http.py $<TARGET_FILE:bench_ev_harness>)
    endif()

    if (WIN32)
        set(_bench_default_python "${CMAKE_CURRENT_SOURCE_DIR}/.venv-bench/Scripts/python.exe")
    else()
        set(_bench_default_python "${CMAKE_CURRENT_SOURCE_DIR}/.venv-bench/bin/python")
    endif()
    set(BENCH_PYTHON "${_bench_default_python}" CACHE FILEPATH "Python with locked offline bench dependencies")
    if (EXISTS "${BENCH_PYTHON}")
        add_test(NAME bench_analysis_regression
            COMMAND "${BENCH_PYTHON}" -m unittest discover -s tests -p test_bench.py -v)
        set_tests_properties(bench_analysis_regression PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 300)
    else()
        message(STATUS "Bench analysis tests unavailable: create .venv-bench or set BENCH_PYTHON")
    endif()

    if (NOT MSVC AND NOT WIN32)
        add_executable(harness_sanitized tests/test_harness.c)
        target_link_libraries(harness_sanitized PRIVATE harness_core)
        target_compile_options(harness_sanitized PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(harness_sanitized PRIVATE -fsanitize=address,undefined)
        add_test(NAME harness_sanitized COMMAND harness_sanitized)
    endif()
endif()

if (BUILD_FUZZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_executable(harness_fuzzer tests/fuzz_harness.c)
    target_link_libraries(harness_fuzzer PRIVATE harness_core)
    target_compile_options(harness_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(harness_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
