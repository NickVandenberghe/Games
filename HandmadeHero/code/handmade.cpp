#include "handmade.h"
#include "handmade_random.h"
#include "handmade_tile.h"

#include "handmade_tile.cpp"

void GameOutputSound(game_state *GameState,
                     game_sound_output_buffer *SoundBuffer, int ToneHz) {
  int16 ToneVolume = 2000;
  int16 *SampleOut = SoundBuffer->Samples;
  int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz; // chunks per second

  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount;
       SampleIndex++) {
#if 0
		real32 SineValue = sinf(GameState->tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
    int16 SampleValue = 0;
#endif
    *SampleOut++ = SampleValue;
    *SampleOut++ = SampleValue;
    // GameState->tSine += (real32)(2.0f * Pi32 * 1.0f) / (real32)WavePeriod;
    // if (GameState->tSine > 2.0f * Pi32)
    // {
    //     GameState->tSine -= (real32)(2.0f * Pi32);
    // }
    //
  }
}

internal void DrawRectangle(game_offscreen_buffer *Buffer, real32 RealMinX,
                            real32 RealMinY, real32 RealMaxX, real32 RealMaxY,
                            real32 R, real32 G, real32 B, real32 Alpha) {
  int32 MinX = RoundReal32ToInt32(RealMinX);
  int32 MinY = RoundReal32ToInt32(RealMinY);
  int32 MaxX = RoundReal32ToInt32(RealMaxX);
  int32 MaxY = RoundReal32ToInt32(RealMaxY);

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

  // Bit pattenr: 0x AA RR GG BB
  uint32 Color = (uint32)((RoundReal32ToUInt32(Alpha * 255.0f) << 24) |
                          (RoundReal32ToUInt32(R * 255.0f) << 16) |
                          (RoundReal32ToUInt32(G * 255.0f) << 8) |
                          (RoundReal32ToUInt32(B * 255.0f) << 0));

  uint8 *Row = (uint8 *)Buffer->Memory + MinX * Buffer->BytesPerPixel +
               MinY * Buffer->Pitch;
  for (int Y = MinY; Y < MaxY; Y++) {
    uint32 *Pixel = (uint32 *)Row;
    for (int X = MinX; X < MaxX; X++) {
      *Pixel++ = Color;
    }
    Row += Buffer->Pitch;
  }
}

#pragma pack(push, 1)
struct bitmap_header {
  uint16 FileType;
  uint32 FileSize;
  uint16 Reserved1;
  uint16 Reserved2;
  uint32 BitmapOffset;
  uint32 Size;
  int32 Width;
  int32 Height;
  uint16 Planes;
  uint16 BitsPerPixel;
};
#pragma pack(pop)

internal uint32 *
DEBUGLoadBitmap(thread_context *Thread,
                debug_platform_read_entire_file *ReadEntireFile,
                char *FileName) {
  uint32 *Result = {};

  debug_read_file_result ReadResult = ReadEntireFile(Thread, FileName);
  if (ReadResult.ContentsSize != 0) {
    bitmap_header *Header = (bitmap_header *)ReadResult.Contents;

    uint32 *Pixels =
        (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
    Result = Pixels;
  }

  return Result;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
  Assert(
      (&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
      (ArrayCount(Input->Controllers[0].Buttons)));

  game_state *GameState = (game_state *)Memory->PermanentStorage;

  real32 PlayerHeight = 1.4f;
  real32 PlayerWidth = 0.75f * PlayerHeight;

  if (!Memory->isInitialized) {
    // char* Filename = __FILE__;
    //
    // debug_read_file_result Result =
    // Memory->DEBUGPlatformReadEntireFile(Thread, Filename); if
    // (Result.Contents)
    // { w
    //     Memory->DEBUGPlatformWriteEntireFile(Thread, "Test.out",
    //     Result.ContentsSize, Result.Contents);
    //     Memory->DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
    // }
    // GameState->ToneHz = 261;
    // GameState->tSine = 0.0f;
    // GameState->PlayerP.X = 100;
    // GameState->PlayerP.Y = 100;

    GameState->PixelPointer =
        DEBUGLoadBitmap(Thread, Memory->DEBUGPlatformReadEntireFile,
                        "../data/test/structured_art.bmp");

    GameState->PlayerP.AbsoluteTileX = 3;
    GameState->PlayerP.AbsoluteTileY = 3;
    GameState->PlayerP.TileRelativeX = 5.0f;
    GameState->PlayerP.TileRelativeY = 5.0f;

    uint32 TilesPerWidth = 17;
    uint32 TilesPerHeight = 9;
    memory_arena *WorldArena = &GameState->WorldArena;

    InitializeArena(WorldArena,
                    Memory->PermanentStorageSize - sizeof(game_state),
                    (uint8 *)Memory->PermanentStorage + sizeof(game_state));

    GameState->World = PushSize(WorldArena, world);
    world *World = GameState->World;
    World->TileMap = PushSize(WorldArena, tile_map);

    tile_map *TileMap = World->TileMap;

    // NOTE: This is set to using 256x256 tile chunks
    TileMap->ChunkShift = 4;
    TileMap->ChunkMask =
        (1 << TileMap->ChunkShift) -
        1; // Mask for ChunkShift bits makes it 0x100 , -1 gives 0xFF
    TileMap->ChunkDim = (1 << TileMap->ChunkShift);

    TileMap->TileChunkCountX = 128;
    TileMap->TileChunkCountY = 128;
    TileMap->TileChunkCountZ = 2;

    TileMap->TileChunks =
        PushArray(WorldArena,
                  TileMap->TileChunkCountX * TileMap->TileChunkCountY *
                      TileMap->TileChunkCountZ,
                  tile_chunk);

    TileMap->TileSideInMeters = 1.4f;

    uint32 ScreenY = 0;
    uint32 ScreenX = 0;
    uint32 RandomNumberIndex = 0;

    bool32 DoorLeft = false;
    bool32 DoorTop = false;
    bool32 DoorRight = false;
    bool32 DoorBottom = false;
    bool32 DoorUp = false;
    bool32 DoorDown = false;
    uint32 AbsTileZ = 0;

    for (uint32 ScreenIndex = 0; ScreenIndex < 100; ScreenIndex++) {
      Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
      uint32 RandomChoice;
      if (DoorUp || DoorDown) {
        RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
      } else {
        RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
      }

      bool32 CreatedZDoor = false;
      if (RandomChoice == 2) {
        CreatedZDoor = true;
        if (AbsTileZ == 0) {
          DoorUp = true;
        } else if (AbsTileZ == 1) {
          DoorDown = true;
        }
      } else if (RandomChoice == 1) {
        DoorRight = true;
      } else {

        DoorTop = true;
      }

      for (uint32 TileY = 0; TileY < TilesPerHeight; TileY++) {
        for (uint32 TileX = 0; TileX < TilesPerWidth; TileX++) {
          uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
          uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;

          uint32 TileValue = 2;
          if ((TileX == 0) && (!DoorLeft || (TileY != (TilesPerHeight / 2)))) {
            TileValue = 3;
          }
          if ((TileX == (TilesPerWidth - 1)) &&
              (!DoorRight || (TileY != (TilesPerHeight / 2)))) {
            TileValue = 3;
          }
          if ((TileY == 0) && (!DoorBottom || (TileX != (TilesPerWidth / 2)))) {
            TileValue = 3;
          }
          if ((TileY == (TilesPerHeight - 1)) &&
              (!DoorTop || (TileX != (TilesPerWidth / 2)))) {
            TileValue = 3;
          }
          if ((TileX == 5) && (TileY == 5)) {
            if (DoorUp) {
              TileValue = 4;
            } else if (DoorDown) {
              TileValue = 5;
            }
          }
          SetTileValue(WorldArena, TileMap, AbsTileX, AbsTileY, AbsTileZ,
                       TileValue);
        }
      }
      DoorLeft = DoorRight;
      DoorBottom = DoorTop;

      if (CreatedZDoor) {
        DoorDown = !DoorDown;
        DoorUp = !DoorUp;
      } else {
        DoorUp = false;
        DoorDown = false;
      }

      DoorTop = false;
      DoorRight = false;

      if (RandomChoice == 2) {
        if (AbsTileZ == 0) {
          AbsTileZ = 1;
        } else if (AbsTileZ == 1) {
          AbsTileZ = 0;
        }
      } else if (RandomChoice == 1) {
        ScreenX += 1;
      } else {
        ScreenY += 1;
      }
    }
    Memory->isInitialized = true;
  }

  world *World = GameState->World;
  tile_map *TileMap = World->TileMap;
  int32 TileSideInPixels = 60;
  real32 MetersToPixels =
      (real32)TileSideInPixels / (real32)TileMap->TileSideInMeters;
  for (int ControllerIndex = 0;
       ControllerIndex < ArrayCount(Input->Controllers); ControllerIndex++) {
    game_controller_input *Controller = GetController(Input, ControllerIndex);
    if (Controller->IsAnalog) {
      // use analog movement tuning
    } else {
      // use digital movement tuning
      real32 dPlayerX = 0.0f;
      real32 dPlayerY = 0.0f;

      if (Controller->MoveUp.EndedDown) {
        dPlayerY = 1.0f;
      }

      if (Controller->MoveDown.EndedDown) {
        dPlayerY = -1.0f;
      }

      if (Controller->MoveLeft.EndedDown) {
        dPlayerX = -1.0f;
      }

      if (Controller->MoveRight.EndedDown) {
        dPlayerX = 1.0f;
      }

      real32 PlayerSpeed = 5.0f;
      if (Controller->LeftShoulder.EndedDown) {
        PlayerSpeed = 10.0f;
      }

      dPlayerX *= PlayerSpeed;
      dPlayerY *= PlayerSpeed;

      // diagnoal will be faster! Fix once we have vectors

      tile_map_position NewPlayerP = GameState->PlayerP;
      NewPlayerP.TileRelativeX += Input->dtForFrame * dPlayerX;
      NewPlayerP.TileRelativeY += Input->dtForFrame * dPlayerY;

      NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);

      tile_map_position PlayerLeft = NewPlayerP;
      PlayerLeft.TileRelativeX -= 0.5f * PlayerWidth;
      PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

      tile_map_position PlayerRight = NewPlayerP;
      PlayerRight.TileRelativeX += 0.5f * PlayerWidth;
      PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);

      bool32 IsValid = IsTileMapPointEmpty(TileMap, NewPlayerP) &&
                       IsTileMapPointEmpty(TileMap, PlayerLeft) &&
                       IsTileMapPointEmpty(TileMap, PlayerRight);

      if (IsValid) {
        if (!AreOnSameTile(&GameState->PlayerP, &NewPlayerP)) {
          uint32 NewTileValue = GetTileValue(TileMap, NewPlayerP);
          if (NewTileValue == 4) {
            NewPlayerP.AbsoluteTileZ++;
          } else if (NewTileValue == 5) {
            NewPlayerP.AbsoluteTileZ--;
          }
        }

        GameState->PlayerP = NewPlayerP;
      }
    }
  }

  DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width,
                (real32)Buffer->Height, 1.0f, 0.0f, 1.0f, 1.0f);

  real32 ScreenCenterY = 0.5f * (real32)Buffer->Height;
  real32 ScreenCenterX = 0.5f * (real32)Buffer->Width;

  for (int32 RelRow = -10; RelRow < 10; RelRow++) {
    for (int32 RelColumn = -20; RelColumn < 20; RelColumn++) {
      uint32 Column = RelColumn + GameState->PlayerP.AbsoluteTileX;
      uint32 Row = RelRow + GameState->PlayerP.AbsoluteTileY;
      uint32 TileId =
          GetTileValue(TileMap, Column, Row, GameState->PlayerP.AbsoluteTileZ);

      if (TileId > 0) {
        real32 Gray = 0.5f;
        if (TileId == 3) {
        }
        switch (TileId) {
        case 3: {
          Gray = 1.0f;
        } break;
        case 4:
        case 5: {
          Gray = 0.25f;
        } break;
        }

        if ((Column == GameState->PlayerP.AbsoluteTileX) &&
            (Row == GameState->PlayerP.AbsoluteTileY)) {
          Gray = 0.0f;
        }

        real32 CenterX = ScreenCenterX -
                         MetersToPixels * GameState->PlayerP.TileRelativeX +
                         ((real32)RelColumn) * TileSideInPixels;
        real32 CenterY = ScreenCenterY +
                         MetersToPixels * GameState->PlayerP.TileRelativeY -
                         ((real32)RelRow) * TileSideInPixels;
        real32 MinX = CenterX - 0.5f * TileSideInPixels;
        real32 MinY = CenterY - 0.5f * TileSideInPixels;
        real32 MaxX = CenterX + 0.5f * TileSideInPixels;
        real32 MaxY = CenterY + 0.5f * TileSideInPixels;

        DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray, Gray);
      }
    }
  }

  real32 PlayerR = 1.0f;
  real32 PlayerG = 1.0f;
  real32 PlayerB = 0.0f;
  real32 PlayerAlpha = 1.0f;

  real32 PlayerLeft = ScreenCenterX - 0.5f * MetersToPixels * PlayerWidth;
  real32 PlayerTop = ScreenCenterY - MetersToPixels * PlayerHeight;

  DrawRectangle(Buffer, PlayerLeft, PlayerTop,
                PlayerLeft + MetersToPixels * PlayerWidth,
                PlayerTop + MetersToPixels * PlayerHeight, PlayerR, PlayerG,
                PlayerB, PlayerAlpha);

#if 0
  uint32 *Source = GameState->PixelPointer;
  uint32 * Dest = (uint32*)Buffer->Memory;
  for (int32 Y = 0; Y < Buffer->Height; Y++) {
    for (int32 X = 0; X < Buffer->Width; X++) {
      *Dest++ = *Source++;
    }
  }
#endif
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
  game_state *GameState = (game_state *)Memory->PermanentStorage;

  GameOutputSound(GameState, SoundBuffer, 400);
}
