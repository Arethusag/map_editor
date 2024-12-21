# Variables
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g 
LIBS = -lsqlite3 -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
TARGET = main
SRC = main.c
DB = test.db


# Default target
all: $(DB) $(TARGET)

# Run setup scripts
$(DB): setup.sh utils/extract_sprites.sh assets/sprites.conf
	./setup.sh

# Build target
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

# Clean target to remove generated files
clean:
	rm -f $(TARGET) $(DB)

# Run target to execute the program
run: $(TARGET)
	./$(TARGET)

# PHONY targets
.PHONY: all clean run
