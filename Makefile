# Author       : Osama Attia (osama.gma@gmail.com)
# Latest update: Thu Sep 16 23:23:02 PDT 2021

SRC_DIR := src
BUILD_DIR := build

TARGET := $(BUILD_DIR)/memsafi.so
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

CXX = g++
FLAGS = -std=c++14 -fPIC -Wall -Wextra -O0 -g -Iinclude
LDFLAGS = -shared
LIBS = -ldl -lpthread

SHELL = /bin/bash
DEPENDENCY_LIST = $(BUILD_DIR)/depend

.PHONY: all clean

all: $(BUILD_DIR) $(DEPENDENCY_LIST) $(TARGET)

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $(FLAGS) -o $(TARGET) $(OBJS) $(LIBS)
	
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(FLAGS) -c $< -o $@

$(DEPENDENCY_LIST): $(SRCS) | $(BUILD_DIR)
	$(RM) $(DEPENDENCY_LIST)
	$(CXX) $(FLAGS) -MM $^ | awk '{print "$(BUILD_DIR)/" $$0;}' >> $(DEPENDENCY_LIST)

# '-' prevents warning on first build or build after clean because dependencies files does not exist
-include $(DEPENDENCY_LIST)

clean:
	$(RM) $(BUILD_DIR)/*.o $(TARGET) $(DEPENDENCY_LIST)
