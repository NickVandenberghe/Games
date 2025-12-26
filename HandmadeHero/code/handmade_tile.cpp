
#include "handmade.h"
inline tile_chunk *GetTileChunk(tile_map *TileMap, uint32 TileChunkX,
                                uint32 TileChunkY, uint32 TileChunkZ) {
  tile_chunk *TileChunk = 0;

  if ((TileChunkX >= 0) && (TileChunkX < TileMap->TileChunkCountX) &&
      (TileChunkY >= 0) && (TileChunkY < TileMap->TileChunkCountY) &&
      (TileChunkZ >= 0) && (TileChunkZ < TileMap->TileChunkCountZ)) {
    TileChunk =
        &TileMap
             ->TileChunks[TileChunkZ * TileMap->TileChunkCountY *
                              TileMap->TileChunkCountX +
                          TileChunkY * TileMap->TileChunkCountX + TileChunkX];
  }
  return TileChunk;
}

inline uint32 GetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk,
                                    uint32 TileX, uint32 TileY) {
  Assert(TileChunk);
  Assert(TileX < TileMap->ChunkDim);
  Assert(TileY < TileMap->ChunkDim);

  uint32 TileChunkValue = TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX];
  return TileChunkValue;
}

inline void SetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk,
                                  uint32 TileX, uint32 TileY,
                                  uint32 TileValue) {
  Assert(TileChunk);
  Assert(TileX < TileMap->ChunkDim);
  Assert(TileY < TileMap->ChunkDim);

  TileChunk->Tiles[TileY * TileMap->ChunkDim + TileX] = TileValue;
}

inline uint32 GetTileValue(tile_map *TileMap, tile_chunk *TileChunk,
                           uint32 TestTileX, uint32 TestTileY) {
  uint32 TileChunkValue = 0;
  if (TileChunk && TileChunk->Tiles) {
    TileChunkValue =
        GetTileValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY);
  }

  return TileChunkValue;
}

inline void SetTileValue(tile_map *TileMap, tile_chunk *TileChunk,
                         uint32 TestTileX, uint32 TestTileY, uint32 TileValue) {
  if (TileChunk && TileChunk->Tiles) {
    SetTileValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY, TileValue);
  }
}

inline tile_chunk_position GetChunkPositionFor(tile_map *TileMap,
                                               uint32 AbsoluteTileX,
                                               uint32 AbsoluteTileY,
                                               uint32 AbsoluteTileZ) {
  tile_chunk_position Result;

  // NOTE: AbsoluteTileX is in first 24 bits of the uint32
  // relative TileX is in the last 8 bits of the uint32
  // This way we can combine tilemap positioning with tile positioning
  Result.TileChunkX = AbsoluteTileX >> TileMap->ChunkShift;
  Result.TileChunkY = AbsoluteTileY >> TileMap->ChunkShift;
  Result.TileChunkZ = AbsoluteTileZ;
  Result.RelativeTileX = AbsoluteTileX & TileMap->ChunkMask;
  Result.RelativeTileY = AbsoluteTileY & TileMap->ChunkMask;

  return Result;
}

internal uint32 GetTileValue(tile_map *TileMap, uint32 AbsTileX,
                             uint32 AbsTileY, uint32 AbsTileZ) {
  tile_chunk_position ChunkPos =
      GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
  tile_chunk *TileChunk = GetTileChunk(
      TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ);
  uint32 TileChunkValue = GetTileValue(
      TileMap, TileChunk, ChunkPos.RelativeTileX, ChunkPos.RelativeTileY);
  return TileChunkValue;
}

internal void SetTileValue(memory_arena *Arena, tile_map *TileMap,
                           uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ,
                           uint32 TileValue) {
  tile_chunk_position ChunkPos =
      GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
  tile_chunk *TileChunk = GetTileChunk(
      TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ);

  if (!TileChunk->Tiles) {
    uint32 TileCount = TileMap->ChunkDim * TileMap->ChunkDim;
    TileChunk->Tiles = PushArray(Arena, TileCount, uint32);
    for (uint32 TileIndex = 0; TileIndex < TileCount; TileIndex++) {
      TileChunk->Tiles[TileIndex] = 1;
    }
  }

  SetTileValue(TileMap, TileChunk, ChunkPos.RelativeTileX,
               ChunkPos.RelativeTileY, TileValue);
}

internal bool32 IsTileMapPointEmpty(tile_map *TileMap,
                                    tile_map_position CanonicalPos) {
  uint32 TileChunkValue =
      GetTileValue(TileMap, CanonicalPos.AbsoluteTileX,
                   CanonicalPos.AbsoluteTileY, CanonicalPos.AbsoluteTileZ);
  bool32 Empty = (TileChunkValue == 2 || TileChunkValue == 4 || TileChunkValue ==5);
  return Empty;
}

inline void RecanonicalizeCoord(tile_map *TileMap, uint32 *Tile,
                                real32 *TileRelative) {
  // NOTE: TileMap is assumed to be toroidal topology. donut-shaped
  // If you walk of 1 edge of map, you enter the otherside of the map

  // however in tiles we have to go to move to next tile
  int32 Offset = RoundReal32ToInt32(*TileRelative / TileMap->TileSideInMeters);
  *Tile += Offset;
  *TileRelative -= Offset * TileMap->TileSideInMeters;

  // need to do something that doesn't use the divide/multiply method
  // for reconanicalizing beause this can end up rounding back on to the tile
  // you just came from

  // add bounds checking to prevent wrappign

  Assert(*TileRelative >= -0.5f * TileMap->TileSideInMeters);
  Assert(*TileRelative <= 0.5f * TileMap->TileSideInMeters);
}

inline tile_map_position RecanonicalizePosition(tile_map *TileMap,
                                                tile_map_position Pos) {
  tile_map_position Result = Pos;

  RecanonicalizeCoord(TileMap, &Result.AbsoluteTileX, &Result.TileRelativeX);
  RecanonicalizeCoord(TileMap, &Result.AbsoluteTileY, &Result.TileRelativeY);

  return Result;
}
