#include "handmade.h"
#include "handmade_intrinsics.h"
#include "handmade_platform.h"

void GameOutputSound(game_state* GameState,
	game_sound_output_buffer* SoundBuffer, int ToneHz) {
	int16 ToneVolume = 2000;
	int16* SampleOut = SoundBuffer->Samples;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz; // chunks per second

	for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount;
		SampleIndex++) {
#if 0
		real32 SineValue = sinf(GameState->tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
		int16 SampleValue = 0;
#endif
		* SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
		// GameState->tSine += (real32)(2.0f * Pi32 * 1.0f) / (real32)WavePeriod;
		// if (GameState->tSine > 2.0f * Pi32)
		// {
		//     GameState->tSine -= (real32)(2.0f * Pi32);
		// }
		//
	}
}
internal void DrawRectangle(game_offscreen_buffer* Buffer, real32 RealMinX,
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

	uint8* Row = (uint8*)Buffer->Memory + MinX * Buffer->BytesPerPixel +
		MinY * Buffer->Pitch;
	for (int Y = MinY; Y < MaxY; Y++) {
		uint32* Pixel = (uint32*)Row;
		for (int X = MinX; X < MaxX; X++) {
			*Pixel++ = Color;
		}
		Row += Buffer->Pitch;
	}
}

internal uint32 GetTileValueUnchecked(world* World, tile_chunk* TileChunk,
	uint32 TileX, uint32 TileY) {
	Assert(TileChunk);
	Assert(TileX < World->ChunkDim);
	Assert(TileY < World->ChunkDim);

	uint32 TileChunkValue = TileChunk->Tiles[TileY * World->ChunkDim + TileX];
	return TileChunkValue;
}

internal uint32 GetTileValue(world* World, tile_chunk* TileChunk, uint32 TestTileX, uint32 TestTileY) {

	uint32 TileChunkValue = 0;
	if (TileChunk) {
		TileChunkValue = GetTileValueUnchecked(World, TileChunk, TestTileX, TestTileY);
	}

	return TileChunkValue;
}

internal tile_chunk* GetTileChunk(world* World, int32 TileChunkX, int32 TileChunkY) {
	tile_chunk* TileChunk = 0;

	if ((TileChunkX >= 0) && (TileChunkX < World->TileChunkCountX) &&
		(TileChunkY >= 0) && (TileChunkY < World->TileChunkCountY)) {
		TileChunk = &World->TileChunks[TileChunkY * World->TileChunkCountX + TileChunkX];
	}
	return TileChunk;
}

inline void RecanonicalizeCoord(world* World, uint32* Tile,
	real32* TileRelative) {
	// NOTE: World is assumed to be toroidal topology. donut-shaped
	// If you walk of 1 edge of map, you enter the otherside of the map

	// however in tiles we have to go to move to next tile
	int32 Offset = FloorReal32ToInt32(*TileRelative / World->TileSideInMeters);
	*Tile += Offset;
	*TileRelative -= Offset * World->TileSideInMeters;

	// need to do something that doesn't use the divide/multiply method
	// for reconanicalizing beause this can end up rounding back on to the tile
	// you just came from

	// add bounds checking to prevent wrappign

	Assert(*TileRelative >= 0);
	Assert(*TileRelative <= World->TileSideInMeters);
}

inline world_position RecanonicalizePosition(world* World, world_position Pos) {
	world_position Result = Pos;

	RecanonicalizeCoord(World, &Result.AbsoluteTileX, &Result.TileRelativeX);
	RecanonicalizeCoord(World, &Result.AbsoluteTileY, &Result.TileRelativeY);

	return Result;
}

inline tile_chunk_position
GetChunkPositionFor(world* World, uint32 AbsoluteTileX, uint32 AbsoluteTileY) {
	tile_chunk_position Result;

	// NOTE: AbsoluteTileX is in first 24 bits of the uint32
	// relative TileX is in the last 8 bits of the uint32
	// This way we can combine tilemap positioning with tile positioning
	Result.TileChunkX = AbsoluteTileX >> World->ChunkShift;
	Result.TileChunkY = AbsoluteTileY >> World->ChunkShift;
	Result.RelativeTileX = AbsoluteTileX & World->ChunkMask;
	Result.RelativeTileY = AbsoluteTileY & World->ChunkMask;

	return Result;
}
internal uint32 GetTileValue(world* World, uint32 AbsTileX, uint32 AbsTileY) {
	tile_chunk_position ChunkPos = GetChunkPositionFor(World, AbsTileX, AbsTileY);
	tile_chunk* TileChunk = GetTileChunk(World, ChunkPos.TileChunkX, ChunkPos.TileChunkY);
	uint32 TileChunkValue = GetTileValue(World, TileChunk, ChunkPos.RelativeTileX, ChunkPos.RelativeTileY);
	return TileChunkValue;
}

// internal uint32 GetTileValue(world *World, world_position CanonicalPos) {
//   bool32 Empty = false;
//   tile_chunk_position ChunkPos = GetChunkPositionFor( World, CanonicalPos.AbsoluteTileX, CanonicalPos.AbsoluteTileY);
//   tile_chunk *TileChunk = GetTileChunk(World, ChunkPos.TileChunkX, ChunkPos.TileChunkY);
//   uint32 TileChunkValue = GetTileValue(World, TileChunk, ChunkPos.RelativeTileX, ChunkPos.RelativeTileY);
//   return TileChunkValue;
// }

internal bool32 IsWorldPointEmpty(world* World, world_position CanonicalPos) {
	uint32 TileChunkValue = GetTileValue(World, CanonicalPos.AbsoluteTileX, CanonicalPos.AbsoluteTileY);
	bool32 Empty = (TileChunkValue == 0);
	return Empty;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	Assert(
		(&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
		(ArrayCount(Input->Controllers[0].Buttons)));

#if 0
#define TILE_MAP_COUNT_X 17
#define TILE_MAP_COUNT_Y 9

	uint32 Tiles00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
		{1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
		{1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
	};

	uint32 Tiles01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
		{1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
		{1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
		{1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
		{1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
	};

	uint32 Tiles10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
		{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
		{1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
		{1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	};

	uint32 Tiles11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
		{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
		{1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
		{1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
		{1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
		{1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	};

	tile_map TileMaps[2][2];
	TileMaps[0][0].Tiles = (uint32*)Tiles00;
	TileMaps[0][1].Tiles = (uint32*)Tiles01;
	TileMaps[1][0].Tiles = (uint32*)Tiles10;
	TileMaps[1][1].Tiles = (uint32*)Tiles11;
#else
#define TILE_MAP_COUNT_X 256
#define TILE_MAP_COUNT_Y 256
	uint32 TempTiles[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,   1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1},
		{1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,   1,0,0,0,1,0,0,0,0,0,0,0,0,1,1,1,1},
		{1,0,1,0,0,0,1,0,0,1,0,0,0,0,0,0,1,   1,0,1,0,1,0,1,0,0,1,0,0,0,0,0,0,1},
		{1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,   0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1},
		{1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1,1,   1,0,0,0,1,0,1,1,1,1,1,1,1,1,0,1,1},
		{1,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,1,   1,0,0,0,1,0,0,0,0,1,0,1,0,0,0,0,1},
		{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,   1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
		{1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,   1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1},

		{1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,   1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1},
		{1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,   1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1},
		{1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,   1,0,0,0,1,0,0,0,0,0,0,0,0,1,1,1,1},
		{1,0,1,0,0,0,1,0,0,1,0,0,0,0,0,0,1,   1,0,1,0,1,0,1,0,0,1,0,0,0,0,0,0,1},
		{1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,   0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1},
		{1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1,1,   1,0,0,0,1,0,1,1,1,1,1,1,1,1,0,1,1},
		{1,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,1,   1,0,0,0,1,0,0,0,0,1,0,1,0,0,0,0,1},
		{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,   1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
	};
#endif
	world World;

	// NOTE: This is set to using 256x256 tile chunks
	World.ChunkShift = 8;
	World.ChunkMask = (1 << World.ChunkShift) - 1;	// Mask for ChunkShift bits makes it 0x100 , -1 gives 0xFF
	World.ChunkDim = 256;

	World.TileChunkCountX = 1;
	World.TileChunkCountY = 1;

	tile_chunk TileChunk = {};
	TileChunk.Tiles = (uint32*)TempTiles;
	World.TileChunks = &TileChunk;

	World.TileSideInMeters = 1.4f;
	World.TileSideInPixels = 60;
	World.MetersToPixels =
		(real32)World.TileSideInPixels / (real32)World.TileSideInMeters;

	real32 LowerLeftX = -(real32)World.TileSideInPixels / 2;
	real32 LowerLeftY = (real32)Buffer->Height;

	real32 PlayerHeight = 1.4f;
	real32 PlayerWidth = 0.75f * PlayerHeight;

	game_state* GameState = (game_state*)Memory->PermanentStorage;

	if (!Memory->isInitialized) {
		// char* Filename = __FILE__;
		//
		// debug_read_file_result Result =
		// Memory->DEBUGPlatformReadEntireFile(Thread, Filename); if
		// (Result.Contents)
		// {
		//     Memory->DEBUGPlatformWriteEntireFile(Thread, "Test.out",
		//     Result.ContentsSize, Result.Contents);
		//     Memory->DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
		// }
		// GameState->ToneHz = 261;
		// GameState->tSine = 0.0f;
		// GameState->PlayerP.X = 100;
		// GameState->PlayerP.Y = 100;

		GameState->PlayerP.AbsoluteTileX = 3;
		GameState->PlayerP.AbsoluteTileY = 3;
		GameState->PlayerP.TileRelativeX = 5.0f;
		GameState->PlayerP.TileRelativeY = 5.0f;

		Memory->isInitialized = true;
	}

	for (int ControllerIndex = 0;
		ControllerIndex < ArrayCount(Input->Controllers); ControllerIndex++) {
		game_controller_input* Controller = GetController(Input, ControllerIndex);
		if (Controller->IsAnalog) {
			// use analog movement tuning
		}
		else {
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

			dPlayerX *= 5.0f;
			dPlayerY *= 5.0f;

			// diagnoal will be faster! Fix once we have vectors

			world_position NewPlayerP = GameState->PlayerP;
			NewPlayerP.TileRelativeX += Input->dtForFrame * dPlayerX;
			NewPlayerP.TileRelativeY += Input->dtForFrame * dPlayerY;

			NewPlayerP = RecanonicalizePosition(&World, NewPlayerP);

			world_position PlayerLeft = NewPlayerP;
			PlayerLeft.TileRelativeX -= 0.5f * PlayerWidth;
			PlayerLeft = RecanonicalizePosition(&World, PlayerLeft);

			world_position PlayerRight = NewPlayerP;
			PlayerRight.TileRelativeX += 0.5f * PlayerWidth;
			PlayerRight = RecanonicalizePosition(&World, PlayerRight);

			bool32 IsValid = IsWorldPointEmpty(&World, NewPlayerP) &&
				IsWorldPointEmpty(&World, PlayerLeft) &&
				IsWorldPointEmpty(&World, PlayerRight);

			if (IsValid) {
				GameState->PlayerP = NewPlayerP;
			}
		}
	}

	DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width,
		(real32)Buffer->Height, 1.0f, 0.0f, 1.0f, 1.0f);

	real32 CenterY = 0.5f * (real32)Buffer->Height;
	real32 CenterX = 0.5f * (real32)Buffer->Width;

	for (int32 RelRow = -10; RelRow < 10; RelRow++) {
		for (int32 RelColumn = -20; RelColumn < 20; RelColumn++) {
			uint32 Column = RelColumn + GameState->PlayerP.AbsoluteTileX;
			uint32 Row = RelRow + GameState->PlayerP.AbsoluteTileY;
			uint32 TileId = GetTileValue(&World, Column, Row);
			real32 Gray = 0.5f;
			if (TileId == 1) {
				Gray = 1.0f;
			}

			if ((Column == GameState->PlayerP.AbsoluteTileX) &&
				(Row == GameState->PlayerP.AbsoluteTileY)) {
				Gray = 0.0f;
			}

			real32 MinX = CenterX - World.MetersToPixels * GameState->PlayerP.TileRelativeX+ ((real32)RelColumn) * World.TileSideInPixels;
			real32 MinY = CenterY + World.MetersToPixels * GameState->PlayerP.TileRelativeY- ((real32)RelRow) * World.TileSideInPixels;
			real32 MaxX = MinX + World.TileSideInPixels;
			real32 MaxY = MinY - World.TileSideInPixels;

			DrawRectangle(Buffer, MinX, MaxY, MaxX, MinY, Gray, Gray, Gray, Gray);
		}
	}

	real32 PlayerR = 1.0f;
	real32 PlayerG = 1.0f;
	real32 PlayerB = 0.0f;
	real32 PlayerAlpha = 1.0f;

	real32 PlayerLeft = CenterX  - 0.5f * World.MetersToPixels * PlayerWidth;
	real32 PlayerTop = CenterY  - World.MetersToPixels * PlayerHeight;

	DrawRectangle(Buffer, PlayerLeft, PlayerTop,
		PlayerLeft + World.MetersToPixels * PlayerWidth,
		PlayerTop + World.MetersToPixels * PlayerHeight, PlayerR,
		PlayerG, PlayerB, PlayerAlpha);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
	game_state* GameState = (game_state*)Memory->PermanentStorage;

	GameOutputSound(GameState, SoundBuffer, 400);
}

// void RenderWeirdGradient(game_offscreen_buffer* Buffer, int XOffset, int
// YOffset)
// {
//     int Width = Buffer->Width;
//     int Height = Buffer->Height;
//
//     uint8* Row = (uint8*)Buffer->Memory; // so that we can move per byte
//
//     for (int Y = 0; Y < Height; Y++)
//     {
//         uint32* Pixel = (uint32*)Row;
//         for (int X = 0; X < Width; X++)
//         {
//             // Pixel in memory: BB GG RR xx                   // little
//             endian
//
//             uint8 Blue = (uint8)(X + XOffset);
//             uint8 Green = (uint8)(Y + YOffset);
//             uint8 Red = (uint8)(X + Y);
//
//             // 0x xx RR GG BB
//             *Pixel++ = ((Red << 16) | (Green << 8) | Blue);
//         }
//
//         Row += Buffer->Pitch;
//     }
// }
//
