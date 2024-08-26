C = gcc
CFLAGS = -Wall -g -lm -g3 -Iinclude
LDFLAGS = -lpthread

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
INCLUDE_DIR = include

jobCommander_src = $(SRC_DIR)/jobCommander.c
jobExecutorServer_src = $(SRC_DIR)/jobExecutorServer.c

jobCommander_obj = $(BUILD_DIR)/jobCommander.o
jobExecutorServer_obj = $(BUILD_DIR)/jobExecutorServer.o

EXECUTABLES = $(BIN_DIR)/jobCommander $(BIN_DIR)/jobExecutorServer

all: $(EXECUTABLES)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/jobCommander: $(jobCommander_obj)
	@mkdir -p $(BIN_DIR)
	$(CC) $(jobCommander_obj) -o $@ $(LDFLAGS)

$(BIN_DIR)/jobExecutorServer: $(jobExecutorServer_obj)
	@mkdir -p $(BIN_DIR)
	$(CC) $(jobExecutorServer_obj) -o $@ $(LDFLAGS)

clean:
	rm -f $(BUILD_DIR)/*.o $(EXECUTABLES)
