#ifndef HANDMADE_H
#include "handmade_platform.h"

/*
 HANDMADE_INTERNAL:
 0 -> Public build
 1 -> Developer build

   HANDMADE_SLOW:
    0 - No slow code allowed!
    1 - Slow code welcome.
*/

#define global_variable static
#define local_persist static
#define internal static

#define Pi32 3.14159265359

#if HANDMADE_SLOW
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else 
#define Assert(Expression)
#endif

#define Kilobytes(Value) (Value *1024LL)
#define Megabytes(Value) (Kilobytes(Value) *1024LL)
#define Gigabytes(Value) (Megabytes(Value) *1024LL)
#define Terabytes(Value) (Gigabytes(Value) *1024LL)

#define ArrayCount(Array) (sizeof(Array)/ sizeof((Array)[0]))

/*
Services that the platform layer provides to the game
*/

inline uint32 SafeTruncateUInt64(uint64 Value) {
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32)Value;

    return Result;
}

inline game_controller_input* GetController(game_input* Input, int ControllerIndex) {
    Assert(ControllerIndex < ArrayCount(Input->Controllers));

    game_controller_input* Result = &Input->Controllers[ControllerIndex];
    return Result;
}

struct world_position
{

#if 0
    int32 TileMapX;
    int32 TileMapY;

    int32 TileX;
    int32 TileY;
#else
    uint32 _TileX;
    uint32 _TileY;
#endif

		// This is tile relative
    real32 TileRelativeX;
    real32 TileRelativeY;
};

struct tile_map
{
    uint32* Tiles;
};

struct world
{
    real32 TileSideInMeters;
    int32 TileSideInPixels;
    real32 MetersToPixels;

    int32 TileCountX;
    int32 TileCountY;

    real32 LowerLeftX;
    real32 LowerLeftY;

    int32 TileMapCountX;
    int32 TileMapCountY;

    tile_map* TileMaps;
};

struct game_state
{
  world_position PlayerP;

    int32 PlayerTileMapX;
    int32 PlayerTileMapY;

    real32 PlayerX;
    real32 PlayerY;
};

#define HANDMADE_H
#endif
