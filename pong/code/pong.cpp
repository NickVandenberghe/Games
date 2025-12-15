#include "pong.h"
#include "pong_platform.h"
#include <iostream>
extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {}

inline int32 RoundReal32ToInt32(real32 Real32) {
  int32 Result = (int32)(Real32 + 0.5f);
  return Result;
}

void draw_rect(game_offscreen_buffer *Buffer, real32 RealMinX, real32 RealMinY,
               real32 RealMaxX, real32 RealMaxY, uint32 color) {
  if (!Buffer || !Buffer->Memory)
    return;

  std::cout << "RealMinX" << RealMinX << '\n';
  std::cout << "RealMaxX" << RealMaxX << '\n';
  std::cout << "RealMinY" << RealMinY << '\n';
  std::cout << "RealMaxY" << RealMaxY << '\n';

  int32 MinX = RoundReal32ToInt32(RealMinX);
  int32 MinY = RoundReal32ToInt32(RealMinY);
  int32 MaxX = RoundReal32ToInt32(RealMaxX);
  int32 MaxY = RoundReal32ToInt32(RealMaxY);

  std::cout << "Before" << '\n';
  std::cout << "MinX" << MinX << '\n';
  std::cout << "MaxX" << MaxX << '\n';
  std::cout << "MinY" << MinY << '\n';
  std::cout << "MaxY" << MaxY << '\n';

  if (MinX < 0) {
    MinX = 0;
  }

  if (MinY < 0) {
    MinY = 0;
  }

  if (MaxX >= Buffer->Width) {
    MaxX = Buffer->Width;
  }

  if (MaxY >= Buffer->Height) {
    MaxY = Buffer->Height;
  }

  std::cout << "After" << '\n';
  std::cout << "MinX" << MinX << '\n';
  std::cout << "MaxX" << MaxX << '\n';
  std::cout << "MinY" << MinY << '\n';
  std::cout << "MaxY" << MaxY << '\n';

  uint8 *Row = (uint8 *)Buffer->Memory + MinX * Buffer->BytesPerPixel +
               MinY * Buffer->Pitch;
  for (int y = MinY; y < MaxY; y++) {
    uint32 *Pixel = (uint32 *)Row;
    for (int x = MinX; x < MaxX; x++)
      Pixel[x] = color;

    Row += Buffer->Pitch;
  }
}

inline bool32 IsCoordinateEmpty() {};

void update_game(game_state *GameState, game_input *GameInput) {

  real32 speed = 250.0f;

  for (int ControllerIndex = 0;
       ControllerIndex < (int)ArrayCount(GameInput->Controllers);
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

      dPlayerX *= speed;
      dPlayerY *= speed;

      GameState->PlayerA.playerY += (int)(GameInput->dtForFrame * dPlayerY);
      // std::cout << "GameState->PlayerA.playerY" << GameState->PlayerA.playerY
      //           << '\n';
      GameState->Ball.playerX += (int)(GameInput->dtForFrame * dPlayerX);
      GameState->Ball.playerY += (int)(GameInput->dtForFrame * dPlayerY);
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

void render_frame(game_state *GameState, game_offscreen_buffer *Buffer) {
  world *World = &GameState->World;
  real32 HorizontalMetersToPixels =
      (real32)Buffer->Width / (real32)World->WorldSideInMeters;

  real32 VerticalMetersToPixels =
      (real32)Buffer->Height / (real32)World->WorldSideInMeters;

  int width = Buffer->Width;
  int height = Buffer->Height;

  std::cout << "width" << width << '\n';
  std::cout << "height" << height << '\n';

  /* fill background */
  uint32_t bg = 0xFF202030; /* dark blue-ish */

  draw_rect(Buffer, 0.0f, 0.0f, (real32)width, (real32)height, bg);

  uint32_t color = 0xFFFFFFFF; // white

  // std::cout << "PlayerA-X" << GameState->PlayerA.playerX << '\n';
  // std::cout << "PlayerA-XMtP"
  //           << GameState->PlayerA.playerX * HorizontalMetersToPixels << '\n';
  // std::cout << "PlayerA-Y" << GameState->PlayerA.playerY << '\n';
  // std::cout << "PlayerA-YMtp"
  // << GameState->PlayerA.playerY * VerticalMetersToPixels << '\n';
  // std::cout << "PlayerB-X" << GameState->PlayerB.playerX << '\n';
  // std::cout << "PlayerB-XMtP"
  //           << GameState->PlayerB.playerX * HorizontalMetersToPixels << '\n';
  // std::cout << "PlayerB-Y" << GameState->PlayerB.playerY << '\n';
  // std::cout << "PlayerB-YMtp"
  //           << GameState->PlayerB.playerY * VerticalMetersToPixels << '\n';
  std::cout << "BallX" << GameState->Ball.playerX << '\n';
  std::cout << "BallXMtP" << GameState->Ball.playerX * HorizontalMetersToPixels
            << '\n';
  std::cout << "BallXMtP"
            << (GameState->Ball.playerX + GameState->BallWidth) *
                   HorizontalMetersToPixels
            << '\n';
  std::cout << "BallY" << GameState->Ball.playerY << '\n';
  std::cout << "BallYMtp" << GameState->Ball.playerY * VerticalMetersToPixels
            << '\n';
  std::cout << "BallYMtp"
            << (GameState->Ball.playerY + GameState->BallHeight) *
                   VerticalMetersToPixels
            << '\n';

  //  player a
  draw_rect(Buffer, GameState->PlayerA.playerX * HorizontalMetersToPixels,
            GameState->PlayerA.playerY * VerticalMetersToPixels,
            (GameState->PlayerA.playerX + GameState->PaddleWidth) *
                HorizontalMetersToPixels,
            (GameState->PlayerA.playerY + GameState->PaddleHeight) *
                VerticalMetersToPixels,
            color);

  // player b
  draw_rect(Buffer, GameState->PlayerB.playerX * HorizontalMetersToPixels,
            GameState->PlayerB.playerY * VerticalMetersToPixels,
            (GameState->PlayerB.playerX + GameState->PaddleWidth) *
                HorizontalMetersToPixels,
            (GameState->PlayerB.playerY + GameState->PaddleHeight) *
                VerticalMetersToPixels,
            color);

  // ball
  draw_rect(Buffer, GameState->Ball.playerX * HorizontalMetersToPixels,
            GameState->Ball.playerY * VerticalMetersToPixels,
            (GameState->Ball.playerX + GameState->BallWidth) *
                HorizontalMetersToPixels,
            (GameState->Ball.playerY + GameState->BallHeight) *
                VerticalMetersToPixels,
            color);
}
