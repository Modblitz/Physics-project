CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:.cpp=.o)
BIN := codec

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.INTERMEDIATE: $(OBJ)

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: clean
