# Variables
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g 
LIBS = -lsqlite3 -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
TARGET = main
SRC = src/main.c src/database.c src/edge.c src/undo.c src/command.c src/grid.c src/draw.c src/window.c src/wall.c
OBJ = $(SRC:.c=.o)
DB = test.db


# Default target
all: $(DB) $(TARGET)

# Run setup scripts
$(DB): utils/setup.sh utils/extract_sprites.sh assets/sprites.conf utils/generate_tables.sh
	./utils/setup.sh

# Build target
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target to remove generated files
clean:
	rm -f $(TARGET) $(DB) $(OBJ)

# Run target to execute the program
run: $(TARGET)
	./$(TARGET)

# PHONY targets
.PHONY: all clean run
