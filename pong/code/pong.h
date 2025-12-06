#ifndef PONG_H
#include "pong_platform.h"

// #define global_variable static
// #define local_persist static
// #define internal static

struct world_position {};

struct game_state {
  world_position PlayerA;
  world_position PlayerB;
};
#define PONG_H
#endif
