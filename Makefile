CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
CPPFLAGS += -Iinclude -MMD -MP
LDLIBS   += -pthread -lcrypto

ifeq ($(OS),Windows_NT)
    LDLIBS += -lws2_32
    EXE := .exe
endif

BUILD := build

COMMON_SRC := $(wildcard src/file/*.cpp) $(wildcard src/network/*.cpp) $(wildcard src/protocol/*.cpp)
CLIENT_SRC := $(wildcard src/client/*.cpp)
SERVER_SRC := $(wildcard src/server/*.cpp)

COMMON_OBJ := $(COMMON_SRC:%.cpp=$(BUILD)/%.o)
CLIENT_OBJ := $(CLIENT_SRC:%.cpp=$(BUILD)/%.o)
SERVER_OBJ := $(SERVER_SRC:%.cpp=$(BUILD)/%.o)

CLIENT_BIN := $(BUILD)/client$(EXE)
SERVER_BIN := $(BUILD)/server$(EXE)

.PHONY: all client server clean

all: client server

client: $(CLIENT_BIN)
server: $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_OBJ) $(COMMON_OBJ)
	$(CXX) $^ -o $@ $(LDLIBS)

$(SERVER_BIN): $(SERVER_OBJ) $(COMMON_OBJ)
	$(CXX) $^ -o $@ $(LDLIBS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(CLIENT_OBJ:.o=.d) $(SERVER_OBJ:.o=.d) $(COMMON_OBJ:.o=.d)