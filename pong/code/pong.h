#ifndef PONG_H
#define PONG_H
#include "pong_platform.h"

// #define global_variable static
// #define local_persist static
// #define internal static

#define Kilobytes(Value) (Value * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

struct world_position {
  int playerX;
  int playerY;
};

struct game_state {
  world_position PlayerA;
  world_position PlayerB;

  world_position Ball;
};

void update_game(game_state *GameState, real32 target_dt);
void render_frame(game_state *GameState, game_offscreen_buffer *buffer);
#endif
