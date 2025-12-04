#ifndef WIN32_HANDMADE_H

struct win32_offscreen_buffer
{
    // Pixels are always 32bits wide, little endian 0x xx RR GG BB or memorder :: BB GG RR xx
    BITMAPINFO Info;
    void* Memory;
    int Width;
    int Height;
    // is in bytes
    int Pitch;
    int BytesPerPixel;
};
struct win32_window_dimension
{
    int Width;
    int Height;
};

struct win32_sound_output
{
    int SamplesPerSecond;
    uint32 RunningSampleIndex;
    int BytesPerSample;
    DWORD SecondaryBufferSize;
    int LatencySampleCount;
    int SafetyBytes;
    // BytesPerSecond
};

struct win32_debug_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;

    DWORD ExpectedFlipPlayCursor;
    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};

struct win32_game_code
{
    HMODULE GameCodeDLL;
    FILETIME DLLLastWriteTime;

    game_update_and_render* UpdateAndRender;
    game_get_sound_samples* GetSoundSamples;

    bool32 IsValid;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

struct win32_replay_buffer
{
    HANDLE FileHandle;
    HANDLE MemoryMap;
    char ReplayFileName[WIN32_STATE_FILE_NAME_COUNT];
    void* MemoryBlock;
};

struct win32_state
{
    uint64 TotalSize;
    void* GameMemoryBlock;
    win32_replay_buffer ReplayBuffers[4];

    HANDLE RecordingHandle;
    int InputRecordingIndex;

    HANDLE PlaybackHandle;
    int InputPlayingIndex;

    char EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
    char* OnePastLastEXEFileNameSlash;
};

struct win32_recorded_input
{
    int InputCount;
    game_input* InputStream;
};

#define WIN32_HANDMADE_H
#endif