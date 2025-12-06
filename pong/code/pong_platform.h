#ifndef PONG_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef float real32;
typedef double real64;

typedef int32 bool32;

typedef struct thread_context {
  int Placeholder;
} thread_context;

typedef struct game_offscreen_buffer {
  // Pixels are always 32bits wide, little endian 0x xx RR GG BB or memorder ::
  // BB GG RR xx BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  // is in bytes

  int BytesPerPixel;
} game_offscreen_buffer;

typedef struct game_memory {
  bool32 isInitialized;
  uint64 PermanentStorageSize;
  void *PermanentStorage; // Required to be cleared at 0 on startup

  uint64 TransientStorageSize;
  void *TransientStorage;

  // debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
  // debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
  // debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
} game_memory;

typedef struct game_input {
} game_input;

typedef struct game_sound_output_buffer {
  int SamplesPerSecond;
  int SampleCount;
  int16 *Samples;
} game_sound_output_buffer;

#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(thread_context *Thread, game_memory *Memory, game_input *Input,    \
            game_offscreen_buffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name)                                           \
  void name(thread_context *Thread, game_memory *Memory,                       \
            game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#ifdef __cplusplus
}
#endif

#define PONG_PLATFORM_H

#endif
