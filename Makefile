# Makefile - fallback build when CMake is not available.
#   make            -> build/cache_sim
#   make test       -> build and run the unit tests
#   make asan       -> tests under AddressSanitizer + UBSan (build-asan/)
#   make run        -> --compare-all with defaults
#   make format     -> clang-format every source file
#   make clean
CXX      ?= g++
BUILD    ?= build
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
EXTRA    ?=
CPPFLAGS := -Iinclude -Ithird_party
LDFLAGS  += -pthread

LIB_SRC  := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
TEST_SRC := $(wildcard tests/*.cpp)
LIB_OBJ  := $(patsubst src/%.cpp,$(BUILD)/obj/%.o,$(LIB_SRC))
TEST_OBJ := $(patsubst tests/%.cpp,$(BUILD)/obj/tests/%.o,$(TEST_SRC))
DEPS     := $(LIB_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(BUILD)/obj/main.d

.PHONY: all test asan run format clean

all: $(BUILD)/cache_sim

$(BUILD)/cache_sim: $(LIB_OBJ) $(BUILD)/obj/main.o
	$(CXX) $(CXXFLAGS) $(EXTRA) $^ -o $@ $(LDFLAGS)

$(BUILD)/unit_tests: $(LIB_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $(EXTRA) $^ -o $@ $(LDFLAGS)

$(BUILD)/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(EXTRA) -MMD -MP -c $< -o $@

$(BUILD)/obj/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(EXTRA) -MMD -MP -c $< -o $@

test: $(BUILD)/unit_tests
	./$(BUILD)/unit_tests

asan:
	$(MAKE) BUILD=build-asan CXXFLAGS="-std=c++17 -O1 -g -Wall -Wextra -Wpedantic" \
	        EXTRA="-fsanitize=address,undefined -fno-omit-frame-pointer" test

run: $(BUILD)/cache_sim
	./$(BUILD)/cache_sim --compare-all

format:
	clang-format -i include/*.h src/*.cpp tests/*.cpp tests/*.h

clean:
	rm -rf build build-asan

-include $(DEPS)
