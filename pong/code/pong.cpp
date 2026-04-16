#include "pong.h"
#include "math.h"
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

  // std::cout << "RealMinX" << RealMinX << '\n';
  // std::cout << "RealMaxX" << RealMaxX << '\n';
  // std::cout << "RealMinY" << RealMinY << '\n';
  // std::cout << "RealMaxY" << RealMaxY << '\n';
  //
  int32 MinX = RoundReal32ToInt32(RealMinX);
  int32 MinY = RoundReal32ToInt32(RealMinY);
  int32 MaxX = RoundReal32ToInt32(RealMaxX);
  int32 MaxY = RoundReal32ToInt32(RealMaxY);

  // std::cout << "Before" << '\n';
  // std::cout << "MinX" << MinX << '\n';
  // std::cout << "MaxX" << MaxX << '\n';
  // std::cout << "MinY" << MinY << '\n';
  // std::cout << "MaxY" << MaxY << '\n';
  //
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

  // std::cout << "After" << '\n';
  // std::cout << "MinX" << MinX << '\n';
  // std::cout << "MaxX" << MaxX << '\n';
  // std::cout << "MinY" << MinY << '\n';
  // std::cout << "MaxY" << MaxY << '\n';
  //
  uint8 *Row = (uint8 *)Buffer->Memory + MinX * Buffer->BytesPerPixel +
               MinY * Buffer->Pitch;
  for (int y = MinY; y < MaxY; y++) {
    uint32 *Pixel = (uint32 *)Row;
    for (int x = MinX; x < MaxX; x++)
      Pixel[x] = color;

    Row += Buffer->Pitch;
  }
}

inline bool32 IsCoordinateEmpty(world *World, vector2 Pos) {
  bool32 Result = false;

  // std::cout << "World->WorldSideInMeters" << World->WorldSideInMeters <<
  // '\n'; std::cout << "0 < Pos.playerX " << (0 < Pos.playerX) << '\n';
  // std::cout << "Pos.playerX < World->WorldSideInMeters"
  //           << (Pos.playerX < World->WorldSideInMeters) << '\n';
  // std::cout << "0 < Pos.playerX " << (0 < Pos.playerY) << '\n';
  // std::cout << "Pos.playerY < World->WorldSideInMeters"
  //           << (Pos.playerY < World->WorldSideInMeters) << '\n';

  if (0 < Pos.X && Pos.X < World->WorldSideInMeters) {
    if (0 < Pos.Y && Pos.Y < World->WorldSideInMeters) {
      Result = true;
    }
  }
  return Result;
};

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

      vector2 Velocity = {0.0f, 0.0f};
      vector2 MoveDirection = {0.0f, 0.0f};

      if (Controller->MoveUp.EndedDown) {
        MoveDirection.Y -= 1.0f;
      }

      if (Controller->MoveDown.EndedDown) {
        MoveDirection.Y += 1.0f;
      }

      if (Controller->MoveLeft.EndedDown) {
        MoveDirection.X -= 1.0f;
      }

      if (Controller->MoveRight.EndedDown) {
        MoveDirection.X += 1.0f;
      }
      real32 Length = sqrtf(MoveDirection.X * MoveDirection.X +
                            MoveDirection.Y * MoveDirection.Y);

      if (Length > 0.0f) {
        MoveDirection.X /= Length;
        MoveDirection.Y /= Length;
      }

      Velocity.X = MoveDirection.X * speed;
      Velocity.Y = MoveDirection.Y * speed;

      entity PlayerPosition = GameState->PlayerA;

      PlayerPosition.Position.Y += (int)(GameInput->dtForFrame * Velocity.Y);
      PlayerPosition.Position.X += (int)(GameInput->dtForFrame * Velocity.X);

      std::cout << "PlayerPosition New Position" << '\n';
      std::cout << "PlayerPosition Position.X" << (PlayerPosition.Position.X)
                << '\n';
      std::cout << "PlayerPosition Position.Y" << (PlayerPosition.Position.Y)
                << '\n'
                << '\n';
      // std::cout << "GameState->PlayerA.playerY" << GameState->PlayerA.playerY
      //           << '\n';
      // GameState->Ball.playerX += (int)(GameInput->dtForFrame * dPlayerX);
      // GameState->Ball.playerY += (int)(GameInput->dtForFrame * dPlayerY);
      // diagnoal will be faster! Fix once we have vectors

      entity PlayerBottom = PlayerPosition;

      PlayerBottom.Position.Y += GameState->PaddleHeight;

      bool32 IsValid =
          IsCoordinateEmpty(&GameState->World, PlayerPosition.Position) &&
          IsCoordinateEmpty(&GameState->World, PlayerBottom.Position);

      if (IsValid) {
        GameState->PlayerA = PlayerPosition;
      }
    }
  }

  entity NewBall = GameState->Ball;

  real32 Min = 0.0f;
  real32 Max = 500.0f;

  std::cout << "NewBall Old Position" << '\n';
  std::cout << "NewBall Position.X" << (NewBall.Position.X) << '\n';
  std::cout << "NewBall Position.Y" << (NewBall.Position.Y) << '\n' << '\n';

  NewBall.Position.X += (GameInput->dtForFrame * NewBall.Velocity.X);
  NewBall.Position.Y += (GameInput->dtForFrame * NewBall.Velocity.Y);

  std::cout << "NewBall New Position" << '\n';
  std::cout << "NewBall Position.X" << (NewBall.Position.X) << '\n';
  std::cout << "NewBall Position.Y" << (NewBall.Position.Y) << '\n' << '\n';

  // X axis bounce
  if (NewBall.Position.X <= Min) {
    NewBall.Position.X = Min;
    NewBall.Velocity.X = -NewBall.Velocity.X;
  } else if (NewBall.Position.X >= Max) {
    NewBall.Position.X = Max;
    NewBall.Velocity.X = -NewBall.Velocity.X;
  }

  // Y axis bounce
  if (NewBall.Position.Y <= Min) {
    NewBall.Position.Y = Min;
    NewBall.Velocity.Y = -NewBall.Velocity.Y;
  } else if (NewBall.Position.Y >= Max) {
    NewBall.Position.Y = Max;
    NewBall.Velocity.Y = -NewBall.Velocity.Y;
  }

  vector2 BallTop = NewBall.Position;
  BallTop.Y -= GameState->BallHeight;

  std::cout << "NewBall after correction " << '\n';
  std::cout << "NewBall Position.X" << (NewBall.Position.X) << '\n';
  std::cout << "NewBall Position.Y" << (NewBall.Position.Y) << '\n' << '\n';

  std::cout << "=============================================================="
            << '\n';
  // bool32 IsValid = IsCoordinateEmpty(&GameState->World, NewBall.Position) &&
  //                  IsCoordinateEmpty(&GameState->World, BallTop);
  //
  // if (IsValid) {
  GameState->Ball = NewBall;
  // }
}

void render_frame(game_state *GameState, game_offscreen_buffer *Buffer) {
  world *World = &GameState->World;
  real32 HorizontalMetersToPixels =
      (real32)Buffer->Width / (real32)World->WorldSideInMeters;

  real32 VerticalMetersToPixels =
      (real32)Buffer->Height / (real32)World->WorldSideInMeters;

  real32 PixelsPerMeter = (HorizontalMetersToPixels < VerticalMetersToPixels)
                              ? HorizontalMetersToPixels
                              : VerticalMetersToPixels;

  int width = Buffer->Width;
  int height = Buffer->Height;

  // std::cout << "width" << width << '\n';
  // std::cout << "HorizontalMetersToPixels" << HorizontalMetersToPixels <<
  // '\n'; std::cout << "height" << height << '\n'; std::cout <<
  // "VerticalMetersToPixels" << VerticalMetersToPixels << '\n';
  //
  /* fill background */
  uint32_t bg = 0xFF202030; /* dark blue-ish */

  draw_rect(Buffer, 0.0f, 0.0f, (real32)width, (real32)height, bg);

  uint32_t color = 0xFFFFFFFF; // white

  std::cout << "PlayerA-X" << GameState->PlayerA.Position.X << '\n';
  std::cout << "PlayerA-XMtP"
            << GameState->PlayerA.Position.X * HorizontalMetersToPixels << '\n';
  std::cout << "PlayerA-Y" << GameState->PlayerA.Position.Y << '\n';
  std::cout << "PlayerA-YMtp"
            << GameState->PlayerA.Position.Y * VerticalMetersToPixels << '\n';
  std::cout << "========" << '\n';
  // std::cout << "PlayerB-X" << GameState->PlayerB.playerX << '\n';
  // std::cout << "PlayerB-XMtP"
  //           << GameState->PlayerB.playerX * HorizontalMetersToPixels << '\n';
  // std::cout << "PlayerB-Y" << GameState->PlayerB.playerY << '\n';
  // std::cout << "PlayerB-YMtp"
  //           << GameState->PlayerB.playerY * VerticalMetersToPixels << '\n';
  // std::cout << "BallX" << GameState->Ball.playerX << '\n';
  // std::cout << "BallXMtP" << GameState->Ball.playerX *
  // HorizontalMetersToPixels
  //           << '\n';
  // std::cout << "BallXMtP"
  //           << (GameState->Ball.playerX + GameState->BallWidth) *
  //                  HorizontalMetersToPixels
  //           << '\n';
  // std::cout << "BallY" << GameState->Ball.playerY << '\n';
  // std::cout << "BallYMtp" << GameState->Ball.playerY * VerticalMetersToPixels
  //           << '\n';
  // std::cout << "BallYMtp"
  //           << (GameState->Ball.playerY + GameState->BallHeight) *
  //                  VerticalMetersToPixels
  //           << '\n';

  //  player a
  draw_rect(Buffer, GameState->PlayerA.Position.X * PixelsPerMeter,
            GameState->PlayerA.Position.Y * PixelsPerMeter,
            (GameState->PlayerA.Position.X + GameState->PaddleWidth) *
                PixelsPerMeter,
            (GameState->PlayerA.Position.Y + GameState->PaddleHeight) *
                PixelsPerMeter,
            color);

  // player b
  draw_rect(Buffer, GameState->PlayerB.Position.X * PixelsPerMeter,
            GameState->PlayerB.Position.Y * PixelsPerMeter,
            (GameState->PlayerB.Position.X + GameState->PaddleWidth) *
                PixelsPerMeter,
            (GameState->PlayerB.Position.Y + GameState->PaddleHeight) *
                PixelsPerMeter,
            color);

  // ball
  draw_rect(
      Buffer, GameState->Ball.Position.X * PixelsPerMeter,
      GameState->Ball.Position.Y * PixelsPerMeter,
      (GameState->Ball.Position.X + GameState->BallWidth) * PixelsPerMeter,
      (GameState->Ball.Position.Y + GameState->BallHeight) * PixelsPerMeter,
      color);
}
