# Makefile for WebServer project

CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread -lmysqlclient 

# Source and object directories
SRC_DIR = code
OBJ_DIR = build
BIN_DIR = bin
TARGET = $(BIN_DIR)/server

# Source files
SOURCES = $(SRC_DIR)/main.cpp \
		  $(SRC_DIR)/config/config.cpp \
		  $(SRC_DIR)/log/log.cpp \
		  $(SRC_DIR)/buffer/buffer.cpp \
		  $(SRC_DIR)/http/httprequest.cpp \
		  $(SRC_DIR)/pool/sqlconnpool.cpp \
		  $(SRC_DIR)/http/httpresponse.cpp \
		  $(SRC_DIR)/http/httpconnect.cpp \
		  $(SRC_DIR)/server/epoller.cpp \
		  $(SRC_DIR)/timer/heaptimer.cpp \
		  $(SRC_DIR)/pool/threadpool.cpp

# Object files
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

# Build target
all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

run: $(TARGET)
	$(TARGET) -p 8080 -t 4

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all run clean