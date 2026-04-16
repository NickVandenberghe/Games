#ifndef PONG_H
#define PONG_H
#include "pong_platform.h"

// #define global_variable static
// #define local_persist static
// #define internal static

#define assert(x)                                                              \
  if (!(x))                                                                    \
    __builtin_trap();

#define Kilobytes(Value) (Value * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

struct world {
  real32 WorldSideInMeters;
  int32 WorldSideInPixels;
  real32 MetersToPixels;

  real32 LowerLeftX;
  real32 LowerLeftY;
};

struct vector2 {
  real32 X;
  real32 Y;
};

struct entity {
  vector2 Position;
  vector2 Velocity;
};

struct game_state {
  entity PlayerA;
  entity PlayerB;

  entity Ball;

  int32 PaddleWidth;
  int32 PaddleHeight;

  int32 BallWidth;
  int32 BallHeight;

  world World;
};

inline game_controller_input *GetController(game_input *Input,
                                            int ControllerIndex) {
  assert(ControllerIndex < (int)ArrayCount(Input->Controllers));

  game_controller_input *Result = &Input->Controllers[ControllerIndex];
  return Result;
}

void update_game(game_state *GameState, game_input *GameInput);
void render_frame(game_state *GameState, game_offscreen_buffer *buffer);
#endif
