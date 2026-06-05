CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O3 -ffast-math -Wall -Wextra

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:.cpp=.o)
BIN := audiocodec

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lm

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.INTERMEDIATE: $(OBJ)

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: clean
