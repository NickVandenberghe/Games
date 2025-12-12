#include "pong.h"
#include "pong_platform.h"
#include <iostream>
extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {}

void draw_rect(game_offscreen_buffer *buffer, int width, int x0, int y0, int w,
               int h, uint32_t color) {
  if (!buffer || !buffer->Memory)
    return;
  if (x0 < 0) {
    w += x0;
    x0 = 0;
  }
  if (y0 < 0) {
    h += y0;
    y0 = 0;
  }
  if (x0 + w > buffer->Width)
    w = buffer->Width - x0;
  if (y0 + h > buffer->Height)
    h = buffer->Height - y0;
  if (w <= 0 || h <= 0)
    return;
  uint8_t *row = (uint8_t *)buffer->Memory + y0 * buffer->Pitch + x0 * 4;
  for (int y = 0; y < h; y++) {
    uint32_t *pixel = (uint32_t *)row;
    for (int x = 0; x < w; x++)
      pixel[x] = color;

    row += buffer->Pitch;
  }
}

void update_game(game_state *GameState, real32 deltaTime) {
  // std::cout << "update_game" << "\n";
  int speed = 200; // pixels per second

  GameState->PlayerA.playerY += (int)(speed * deltaTime);
  GameState->PlayerB.playerY += (int)(speed * deltaTime);
}

void render_frame(game_state *GameState, game_offscreen_buffer *buffer) {

  int width = buffer->Width;
  int height = buffer->Height;

  /* fill background */
  uint32_t bg = 0xFF202030; /* dark blue-ish */

  draw_rect(buffer, width, 0, 0, width, height, bg);

  uint32_t color = 0xFFFFFFFF; // white

  //  player a
  draw_rect(buffer, width, GameState->PlayerA.playerX,
            GameState->PlayerA.playerY, 30, height / 2, color);
  // player b
  draw_rect(buffer, width, GameState->PlayerB.playerX,
            GameState->PlayerB.playerY, 30, height / 2, color);
  // ball
  draw_rect(buffer, width, 320, 320, 30, 30, color);
}
