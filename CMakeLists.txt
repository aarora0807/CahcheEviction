cmake_minimum_required(VERSION 3.15)
project(CacheEvictionSimulator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(CACHESIM_SANITIZE "Build with AddressSanitizer + UBSan" OFF)
option(CACHESIM_TESTS "Build the unit tests" ON)

find_package(Threads REQUIRED)

# ---- core library (everything except main) ----
add_library(cachesim_core
  src/AIPolicy.cpp src/AdaptivePolicy.cpp src/BeladyPolicy.cpp src/Cache.cpp
  src/Cli.cpp src/FIFOPolicy.cpp src/LFUPolicy.cpp src/LRUPolicy.cpp src/Metrics.cpp
  src/PolicyFactory.cpp src/PolicyTrainer.cpp src/ResultExporter.cpp src/Simulator.cpp
  src/Tracer.cpp src/WebExporter.cpp src/Workload.cpp src/Workloads.cpp)
target_include_directories(cachesim_core PUBLIC include)
target_link_libraries(cachesim_core PUBLIC Threads::Threads)

if(MSVC)
  target_compile_options(cachesim_core PUBLIC /W4 /permissive- /EHsc /utf-8)
  target_compile_definitions(cachesim_core PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  target_compile_options(cachesim_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

if(CACHESIM_SANITIZE AND NOT MSVC)
  target_compile_options(cachesim_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer -g)
  target_link_options(cachesim_core PUBLIC -fsanitize=address,undefined)
endif()

# Put cache_sim.exe / unit_tests.exe straight into build/ (not build/Release/) on every platform.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER ${cfg} CFG)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} ${CMAKE_BINARY_DIR})
endforeach()

# ---- simulator executable ----
add_executable(cache_sim src/main.cpp)
target_link_libraries(cache_sim PRIVATE cachesim_core)

# ---- unit tests (doctest, vendored single header) ----
if(CACHESIM_TESTS)
  enable_testing()
  add_executable(unit_tests
    tests/test_main.cpp tests/test_fifo.cpp tests/test_lru.cpp tests/test_lfu.cpp
    tests/test_cache.cpp tests/test_workloads.cpp tests/test_trainer.cpp)
  target_include_directories(unit_tests PRIVATE third_party tests)
  target_link_libraries(unit_tests PRIVATE cachesim_core)
  add_test(NAME unit_tests COMMAND unit_tests)
endif()
