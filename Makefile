CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -IUI/includes -IGame/includes
LIBS = -lsfml-graphics -lsfml-window -lsfml-system

SRCS = main.cpp $(wildcard UI/scr/*.cpp) $(wildcard Game/scr/*.cpp)
TARGET = test_game

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) $(LIBS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

# -----------------------------------------------------------------------------
# Headless tests: make test
# Builds every scratch/test_*.cpp against the engine only (Game/scr/*.cpp) and runs it.
# SFML is not needed: scratch/sfml_stub replaces the two SFML types the engine uses.
# A failing test stops the run (make -k test runs all of them).
# Replay a run with the same weather: make test EC_SEED=42
# -----------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
EXE = .exe
else
EXE =
endif

TEST_DIR = scratch
TEST_BIN_DIR = $(TEST_DIR)/bin
TEST_STUB_DIR = $(TEST_DIR)/sfml_stub
TEST_CXXFLAGS = -std=c++17 -Wall -Wextra -I$(TEST_STUB_DIR) -IGame/includes -include $(TEST_STUB_DIR)/compat_cxx17.h

TEST_SRCS = $(sort $(wildcard $(TEST_DIR)/test_*.cpp))
TEST_NAMES = $(basename $(notdir $(TEST_SRCS)))
TEST_BINS = $(addprefix $(TEST_BIN_DIR)/,$(addsuffix $(EXE),$(TEST_NAMES)))
TEST_RUNS = $(addprefix run-,$(TEST_NAMES))
TEST_HDRS = $(wildcard Game/includes/*.h) $(wildcard $(TEST_STUB_DIR)/*.h) $(wildcard $(TEST_STUB_DIR)/SFML/*.hpp)
ENGINE_SRCS = $(wildcard Game/scr/*.cpp)
ENGINE_OBJS = $(addprefix $(TEST_BIN_DIR)/,$(notdir $(ENGINE_SRCS:.cpp=.o)))

test: $(TEST_RUNS)
	@echo All $(words $(TEST_RUNS)) test programs passed.

$(TEST_RUNS): run-%: $(TEST_BIN_DIR)/%$(EXE)
	./$<

$(TEST_BINS): $(TEST_BIN_DIR)/%$(EXE): $(TEST_DIR)/%.cpp $(ENGINE_OBJS) $(TEST_HDRS)
	$(CXX) $(TEST_CXXFLAGS) $< $(ENGINE_OBJS) -o $@

$(ENGINE_OBJS): $(TEST_BIN_DIR)/%.o: Game/scr/%.cpp $(TEST_HDRS) | $(TEST_BIN_DIR)
	$(CXX) $(TEST_CXXFLAGS) -c $< -o $@

$(TEST_BIN_DIR):
	mkdir -p $(TEST_BIN_DIR)

clean:
	rm -f $(TARGET)
	rm -rf $(TEST_BIN_DIR)

.PHONY: all run clean test $(TEST_RUNS)
