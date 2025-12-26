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

#define Kilobytes(Value) (Value * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

/*
Services that the platform layer provides to the game
*/

inline uint32 SafeTruncateUInt64(uint64 Value) {
  Assert(Value <= 0xFFFFFFFF);
  uint32 Result = (uint32)Value;

  return Result;
}

inline game_controller_input *GetController(game_input *Input,
                                            int ControllerIndex) {
  Assert(ControllerIndex < ArrayCount(Input->Controllers));

  game_controller_input *Result = &Input->Controllers[ControllerIndex];
  return Result;
}

struct memory_arena {
  memory_index Size;

  uint8* Base;
  memory_index Used;
};

inline void InitializeArena(memory_arena *Arena, memory_index Size, uint8 *Base) {
  Arena->Size = Size;
  Arena->Base = Base;
  Arena->Used = 0;
}

#define PushSize(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, count, type)                                          \
  (type *)PushSize_(Arena, (count) * sizeof(type))

void *PushSize_(memory_arena *Arena, memory_index Size) {
  Assert((Arena->Used + Size) <= Arena->Size);
  void *Result = Arena->Base + Arena->Used;
  Arena->Used += Size;

  return Result;
}

#include "handmade_intrinsics.h"
#include "handmade_tile.h"


struct world {
  tile_map *TileMap;
};

struct game_state {
  memory_arena WorldArena;
  tile_map_position PlayerP;
  world *World;
};

#define HANDMADE_H
#endif
