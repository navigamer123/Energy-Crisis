# =============================================================================
# Energy Crisis - Makefile (GNU make; Linux and Windows/MinGW)
#
#   make                 build the game  -> ./test_game  (test_game.exe on Windows)
#   make run             build and start the game
#   make test            build and run the headless engine tests (no SFML needed)
#   make clean           remove everything the Makefile built
#
# Linux:    SFML 3 installed system-wide (or: make SFML_DIR=/path/to/SFML-3.x)
# Windows:  mingw32-make SFML_DIR=C:/SFML-3.1.0   from Git Bash, MSYS2, cmd or PowerShell
#           (the SFML DLLs are copied next to test_game.exe)
#
# Variables: SFML_DIR=<SFML 3 folder with include/ and lib/>  CXX=<compiler>  BUILD_DIR=<dir>
#            EC_SEED=<n> (same weather in every run, e.g. make test EC_SEED=42)
# The CMake build (CMakeLists.txt) builds the same targets and the same tests; see docs/BUILD.md.
# =============================================================================

CXX       = g++
CXXFLAGS  = -std=c++17 -O2 -Wall -Wextra
CPPFLAGS  = -IUI/includes -IGame/includes
DEPFLAGS  = -MMD -MP
LDFLAGS   =
LIBS      = -lsfml-audio -lsfml-graphics -lsfml-window -lsfml-system
BUILD_DIR = build/make
TARGET    = test_game

ifdef SFML_DIR
CPPFLAGS += -I$(SFML_DIR)/include
LDFLAGS  += -L$(SFML_DIR)/lib
ifneq ($(OS),Windows_NT)
# Linux/macOS: find the SFML shared libraries at run time without LD_LIBRARY_PATH
LDFLAGS  += -Wl,-rpath,$(SFML_DIR)/lib
endif
endif

# Same weather in every run when EC_SEED is given (game and tests read it from the environment)
ifdef EC_SEED
export EC_SEED
endif

# -----------------------------------------------------------------------------
# Platform and shell helpers
# mingw32-make runs commands with sh.exe when one is on PATH (Git Bash, MSYS2), else with cmd.exe;
# cmd.exe keeps the quotes in: echo "sh". Parallel jobs (-j) may race to create a folder: that is not an error.
# -----------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
EXE = .exe
ifeq ($(shell echo "sh"),"sh")
WIN_CMD = 1
endif
else
EXE =
endif

ifdef WIN_CMD
winpath = $(subst /,\,$(1))
MKDIR_P = if not exist "$(call winpath,$(1))" mkdir "$(call winpath,$(1))" 2>nul || if exist "$(call winpath,$(1))" ver >nul
RM_RF   = if exist "$(call winpath,$(1))" rmdir /s /q "$(call winpath,$(1))"
RM_F    = del /f /q $(call winpath,$(1)) 2>nul
CP_F    = copy /y "$(call winpath,$(1))" "$(call winpath,$(2))" >nul
RUN     = $(call winpath,$(1))
else
MKDIR_P = mkdir -p $(1)
RM_RF   = rm -rf $(1)
RM_F    = rm -f $(1)
CP_F    = cp -f $(1) $(2)
RUN     = ./$(1)
endif

# -----------------------------------------------------------------------------
# The game: main.cpp + UI/scr/*.cpp + Game/scr/*.cpp, objects (with header dependencies) in $(BUILD_DIR)
# -----------------------------------------------------------------------------
GAME_SRCS = main.cpp $(wildcard UI/scr/*.cpp) $(wildcard Game/scr/*.cpp)
GAME_OBJS = $(addprefix $(BUILD_DIR)/game/,$(GAME_SRCS:.cpp=.o))
GAME_BIN  = $(TARGET)$(EXE)

# Windows with SFML_DIR: copy the SFML DLLs next to the game so it starts from the project folder
SFML_DLLS =
SFML_DLL_NAMES =
ifeq ($(OS),Windows_NT)
SFML_DLL_NAMES = $(foreach m,audio graphics window system,sfml-$(m)-3.dll)
ifdef SFML_DIR
SFML_DLLS = $(SFML_DLL_NAMES)
endif
endif

all: $(GAME_BIN) $(SFML_DLLS)

$(GAME_BIN): $(GAME_OBJS)
	$(CXX) $(LDFLAGS) $(GAME_OBJS) $(LIBS) -o $@

$(BUILD_DIR)/game/%.o: %.cpp
	@$(call MKDIR_P,$(@D))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

ifneq ($(SFML_DLLS),)
$(SFML_DLLS): sfml-%-3.dll: $(SFML_DIR)/bin/sfml-%-3.dll
	$(call CP_F,$<,$@)
endif

ifneq ($(EXE),)
# "make test_game" also works on Windows, where the file is test_game.exe
$(TARGET): $(GAME_BIN)
.PHONY: $(TARGET)
endif

# The game finds assets/ next to the executable or in the working directory (the project folder)
run: all
	$(call RUN,$(GAME_BIN))

# -----------------------------------------------------------------------------
# Headless engine tests: make test
# Builds every scratch/test_*.cpp against the engine only (Game/scr/*.cpp) and runs it - the same
# programs CMake registers with CTest. SFML is not needed: scratch/sfml_stub replaces the two SFML
# types the engine uses. A failing test stops the run (make -k test runs all of them).
# -----------------------------------------------------------------------------
TEST_DIR       = scratch
TEST_STUB_DIR  = $(TEST_DIR)/sfml_stub
TEST_BIN_DIR   = $(BUILD_DIR)/tests
TEST_OBJ_DIR   = $(TEST_BIN_DIR)/engine
# compat_cxx17.h back-fills std::clamp for old compilers such as MinGW g++ 6.3
TEST_CPPFLAGS  = -I$(TEST_STUB_DIR) -IGame/includes -include $(TEST_STUB_DIR)/compat_cxx17.h
TEST_CXXFLAGS  = -std=c++17 -Wall -Wextra

TEST_SRCS    = $(sort $(wildcard $(TEST_DIR)/test_*.cpp))
TEST_NAMES   = $(basename $(notdir $(TEST_SRCS)))
TEST_BINS    = $(addprefix $(TEST_BIN_DIR)/,$(addsuffix $(EXE),$(TEST_NAMES)))
TEST_RUNS    = $(addprefix run-,$(TEST_NAMES))
ENGINE_SRCS  = $(wildcard Game/scr/*.cpp)
ENGINE_OBJS  = $(addprefix $(TEST_OBJ_DIR)/,$(notdir $(ENGINE_SRCS:.cpp=.o)))

test: $(TEST_RUNS)
	@echo All $(words $(TEST_RUNS)) test programs passed.

tests: $(TEST_BINS)

$(TEST_RUNS): run-%: $(TEST_BIN_DIR)/%$(EXE)
	$(call RUN,$<)

$(TEST_BINS): $(TEST_BIN_DIR)/%$(EXE): $(TEST_DIR)/%.cpp $(ENGINE_OBJS)
	$(CXX) $(TEST_CPPFLAGS) $(TEST_CXXFLAGS) $(DEPFLAGS) -MF $(TEST_BIN_DIR)/$*.d -MT $@ $< $(ENGINE_OBJS) -o $@

$(ENGINE_OBJS): $(TEST_OBJ_DIR)/%.o: Game/scr/%.cpp
	@$(call MKDIR_P,$(@D))
	$(CXX) $(TEST_CPPFLAGS) $(TEST_CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# -----------------------------------------------------------------------------
clean:
	$(call RM_RF,$(BUILD_DIR))
	$(call RM_RF,$(TEST_DIR)/bin)
	-$(call RM_F,$(GAME_BIN) $(SFML_DLL_NAMES))

-include $(GAME_OBJS:.o=.d) $(ENGINE_OBJS:.o=.d) $(TEST_BINS:$(EXE)=.d)

.PHONY: all run clean test tests $(TEST_RUNS)
