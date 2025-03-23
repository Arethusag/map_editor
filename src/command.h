// command.h
#ifndef COMMAND_H
#define COMMAND_H

// includes
#include "database.h"
#include <sqlite3.h>

// globals
extern char command[256];
extern unsigned int commandIndex;
extern bool inCommandMode;
extern int activeTileKey;

// functions

void parseCommand(Tile tileTypes[], Edge edgeTypes[], sqlite3 *db);

void handleCommandMode(char *command, unsigned int *commandIndex,
                       bool *inCommandMode, int screenHeight, int screenWidth,
                       Tile tileTypes[], Edge edgeTypes[], sqlite3 *db);

#endif // COMMAND_H
