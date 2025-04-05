// command.c
#include "command.h"
#include "draw.h"
#include "edge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void parseCommand(Tile tileTypes[], Edge edgeTypes[], sqlite3 *db,
                  drawingState *drawState) {
  if (strncmp(command, ":tile ", 6) == 0) {
    char *tileKeyStr = command + 6;
    char *endptr;
    long tileKey = strtol(tileKeyStr, &endptr, 10);
    if (*endptr == '\0') {
      if (tileKey >= 0 && tileKey <= maxTileKey &&
          tileTypes[tileKey].tileKey == tileKey) {
        drawState->activeTileKey = (int)tileKey;
        printf("Active tile set to %d\n", drawState->activeTileKey);
      } else {
        printf("Tile key out of range\n");
      }
    } else {
      printf("Invalid tile key\n");
    }
  } else if (strncmp(command, ":load ", 6) == 0) {
    char *table = &command[6];
    loadMap(db, table);
    computeMapEdges(tileTypes, edgeTypes);
    printf("Map loaded: %s\n", table);
  } else if (strncmp(command, ":save ", 6) == 0) {
    char *table = &command[6];
    saveMap(db, table);
    printf("Map saved: %s\n", table);
  } else {
    printf("Command not recognized\n");
  }
}

void handleCommandMode(char *command, unsigned int *commandIndex,
                       bool *inCommandMode, int screenHeight, int screenWidth,
                       Tile tileTypes[], Edge edgeTypes[], sqlite3 *db,
                       drawingState *drawState) {

  // Command mode entry
  if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
    if (IsKeyPressed(KEY_SEMICOLON)) {
      printf("Entering command mode\n");
      *inCommandMode = true;
      *commandIndex = 0;
      memset(command, 0, 256);
    }
  }

  if (*inCommandMode) {
    // Handle character input
    int key = GetCharPressed();
    while (key > 0) { // Process all queued characters
      if (key >= 32 && key <= 126 && *commandIndex < 255) {
        command[*commandIndex] = (char)key;
        (*commandIndex)++;
        command[*commandIndex] = '\0';
      }
      key = GetCharPressed(); // Get next character in queue
    }

    // Handle backspace
    if (IsKeyPressed(KEY_BACKSPACE) && *commandIndex > 0) {
      (*commandIndex)--;
      command[*commandIndex] = '\0';
    }

    // Handle command execution or exit
    if (IsKeyPressed(KEY_ENTER)) {
      printf("Command entered: %s\n", command);
      parseCommand(tileTypes, edgeTypes, db, drawState);
      *inCommandMode = false;
    } else if (IsKeyPressed(KEY_ESCAPE)) {
      *inCommandMode = false;
    }

    DrawRectangle(0, screenHeight - 30, screenWidth, 30, DARKGRAY);
    DrawText(command, 10, screenHeight - 25, 20, RAYWHITE);
  }
}
