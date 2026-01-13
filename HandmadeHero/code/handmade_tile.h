#ifndef HANDMADE_TILE_H
struct tile_map_position {
  // These are fixed point tile locations
  // The high bits are the tile chunk index
  // the low bits are the tile index in the chunk
  uint32 AbsoluteTileX;
  uint32 AbsoluteTileY;
  uint32 AbsoluteTileZ;

  // This is tile relative
  real32 TileRelativeX;
  real32 TileRelativeY;
};

struct tile_chunk_position {
  uint32 TileChunkX;
  uint32 TileChunkY;
  uint32 TileChunkZ;

  uint32 OffsetX;
  uint32 OffsetY;
};

struct tile_chunk {
  uint32 *Tiles;
};

struct tile_map {
  uint32 ChunkShift;
  uint32 ChunkMask;
  /// <summary>
  /// Chunk Dimension
  /// </summary>
  uint32 ChunkDim;

  real32 TileSideInMeters;

  uint32 TileChunkCountX;
  uint32 TileChunkCountY;
  uint32 TileChunkCountZ;

  //TODO: we need real sparseness so anywhere in the world can be 
  //represented wihtout the giant pointer array
  tile_chunk *TileChunks;
};

#define HANDMADE_TILE_H
#endif
