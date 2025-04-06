// command.c
#include "command.h"
#include "draw.h"
#include "edge.h"
#include "wall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void parseCommand(Tile tileTypes[], Edge edgeTypes[], Wall wallTypes[],
                  sqlite3 *db, DrawingState *drawState,
                  CommandState *commandState, Map *map) {

  if (strncmp(commandState->commandBuffer, ":tile ", 6) == 0) {
    if (drawState->drawType != DRAW_TILE) {
      drawState->drawType = DRAW_TILE;
      puts("Updated draw type to tile");
    }
    char *tileKeyStr = commandState->commandBuffer + 6;
    char *endptr;
    long tileKey = strtol(tileKeyStr, &endptr, 10);
    if (*endptr == '\0') {
      if (tileKey >= 0 && tileKey <= map->maxTileKey &&
          tileTypes[tileKey].tileKey == tileKey) {
        drawState->activeTileKey = (int)tileKey;
        printf("Active tile set to %d\n", drawState->activeTileKey);
      } else {
        printf("Tile key out of range\n");
      }
    } else {
      printf("Invalid tile key\n");
    }
  } else if (strncmp(commandState->commandBuffer, ":wall ", 6) == 0) {
    if (drawState->drawType != DRAW_WALL) {
      drawState->drawType = DRAW_WALL;
      puts("Updated draw type to wall");
    }
    char *wallKeyStr = commandState->commandBuffer + 6;
    char *endptr;
    long wallKey = strtol(wallKeyStr, &endptr, 10);
    if (*endptr == '\0') {
      if (wallKey >= 0 && wallKey <= map->maxWallKey &&
          wallTypes[wallKey].wallKey == wallKey) {
        drawState->activeWallKey = (int)wallKey;
        printf("Active wall set to %d\n", drawState->activeWallKey);
      } else {
        printf("Wall key out of range\n");
      }
    } else {
      printf("Invalid wall key\n");
    }
  } else if (strncmp(commandState->commandBuffer, ":load ", 6) == 0) {
    char *table = &commandState->commandBuffer[6];
    loadMap(db, table, map);
    computeMapEdges(tileTypes, edgeTypes, map);
    computeMapWalls(wallTypes, map);
    printf("Map loaded: %s\n", table);
  } else if (strncmp(commandState->commandBuffer, ":save ", 6) == 0) {
    char *table = &commandState->commandBuffer[6];
    saveMap(db, table, map);
    printf("Map saved: %s\n", table);
  } else {
    printf("Command not recognized\n");
  }
}

void handleCommandMode(CommandState *commandState, int screenHeight,
                       int screenWidth, Tile tileTypes[], Edge edgeTypes[],
                       Wall wallTypes[], sqlite3 *db, DrawingState *drawState,
                       Map *map) {

  // Command mode entry
  if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
    if (IsKeyPressed(KEY_SEMICOLON)) {
      printf("Entering command mode\n");
      commandState->inCommandMode = true;
      commandState->commandIndex = 0;
      memset(commandState->commandBuffer, 0, 256);
    }
  }

  if (commandState->inCommandMode) {
    // Handle character input
    int key = GetCharPressed();
    while (key > 0) { // Process all queued characters
      if (key >= 32 && key <= 126 && commandState->commandIndex < 255) {
        commandState->commandBuffer[commandState->commandIndex] = (char)key;
        (commandState->commandIndex)++;
        commandState->commandBuffer[commandState->commandIndex] = '\0';
      }
      key = GetCharPressed(); // Get next character in queue
    }

    // Handle backspace
    if (IsKeyPressed(KEY_BACKSPACE) && commandState->commandIndex > 0) {
      (commandState->commandIndex)--;
      commandState->commandBuffer[commandState->commandIndex] = '\0';
    }

    // Handle command execution or exit
    if (IsKeyPressed(KEY_ENTER)) {
      printf("Command entered: %s\n", commandState->commandBuffer);
      parseCommand(tileTypes, edgeTypes, wallTypes, db, drawState, commandState,
                   map);
      commandState->inCommandMode = false;
    } else if (IsKeyPressed(KEY_ESCAPE)) {
      commandState->inCommandMode = false;
    }

    DrawRectangle(0, screenHeight - 30, screenWidth, 30, DARKGRAY);
    DrawText(commandState->commandBuffer, 10, screenHeight - 25, 20, RAYWHITE);
  }
}
