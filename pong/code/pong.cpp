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

void update_game(game_state *GameState, game_input *GameInput,
                 real32 deltaTime) {
  // std::cout << "update_game" << "\n";
  int speed = 2; // pixels per second

  // std::cout << "update_game" << '\n';
  for (int ControllerIndex = 0;
       ControllerIndex < ArrayCount(GameInput->Controllers);
       ControllerIndex++) {
    game_controller_input *Controller =
        GetController(GameInput, ControllerIndex);
    if (Controller->IsAnalog) {
      // use analog movement tuning
    } else {
      // use digital movement tuning
      real32 dPlayerX = 0.0f;
      real32 dPlayerY = 0.0f;

      if (Controller->MoveUp.EndedDown) {
        dPlayerY = -1.0f;
      }

      if (Controller->MoveDown.EndedDown) {
        dPlayerY = 1.0f;
      }

      if (Controller->MoveLeft.EndedDown) {
        dPlayerX = -1.0f;
      }

      if (Controller->MoveRight.EndedDown) {
        dPlayerX = 1.0f;
      }

      dPlayerX *= 5.0f;
      dPlayerY *= 5.0f;

      GameState->PlayerA.playerY += (int)(speed * dPlayerY);
      GameState->Ball.playerX += (int)(speed * dPlayerX);
      GameState->Ball.playerY += (int)(speed * dPlayerY);
      // diagnoal will be faster! Fix once we have vectors

      // world_position NewPlayerP = GameState->PlayerP;
      // NewPlayerP.TileRelativeX += Input->dtForFrame * dPlayerX;
      // NewPlayerP.TileRelativeY += Input->dtForFrame * dPlayerY;
      //
      // NewPlayerP = RecanonicalizePosition(&World, NewPlayerP);
      //
      // world_position PlayerLeft = NewPlayerP;
      // PlayerLeft.TileRelativeX -= 0.5f * PlayerWidth;
      // PlayerLeft = RecanonicalizePosition(&World, PlayerLeft);
      //
      // world_position PlayerRight = NewPlayerP;
      // PlayerRight.TileRelativeX += 0.5f * PlayerWidth;
      // PlayerRight = RecanonicalizePosition(&World, PlayerRight);

      // bool32 IsValid = IsWorldPointEmpty(&World, NewPlayerP) &&
      //                  IsWorldPointEmpty(&World, PlayerLeft) &&
      //                  IsWorldPointEmpty(&World, PlayerRight);
      //
      // if (IsValid) {
      //   GameState->PlayerP = NewPlayerP;
      // }
    }
  }
  // GameState->PlayerA.playerY += (int)(speed * deltaTime);
  // GameState->PlayerB.playerY += (int)(speed * deltaTime);
}

void render_frame(game_state *GameState, game_input *GameInput,
                  game_offscreen_buffer *buffer) {

  int width = buffer->Width;
  int height = buffer->Height;

  std::cout << "GameState->Ball.playerX" << GameState->Ball.playerX << '\n';
  std::cout << "GameState->Ball.playerY" << GameState->Ball.playerY << '\n';

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
  draw_rect(buffer, width, GameState->Ball.playerX, GameState->Ball.playerY, 30,
            30, color);
}
