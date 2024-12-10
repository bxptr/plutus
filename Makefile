# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -pedantic -O2 -pthread

# Directories
SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin

# Output executable
TARGET := $(BIN_DIR)/exchange

# Source files
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC_FILES))

# Rules
all: $(TARGET)

$(TARGET): $(OBJ_FILES) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run

