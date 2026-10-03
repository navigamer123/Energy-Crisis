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

clean:
	rm -f $(TARGET)

.PHONY: all run clean
