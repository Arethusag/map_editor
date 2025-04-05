// command.h
#ifndef COMMAND_H
#define COMMAND_H

#include "database.h"
#include "draw.h"
#include <sqlite3.h>

typedef struct {
  char commandBuffer[256];
  unsigned int commandIndex;
  bool inCommandMode;
} CommandState;

void parseCommand(Tile tileTypes[], Edge edgeTypes[], sqlite3 *db,
                  drawingState *drawState, CommandState *commandState);

void handleCommandMode(CommandState *commandState, int screenHeight,
                       int screenWidth, Tile tileTypes[], Edge edgeTypes[],
                       sqlite3 *db, drawingState *drawState);

#endif // COMMAND_H
