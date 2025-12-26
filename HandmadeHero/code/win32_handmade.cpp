
/*
NOT FINAL PLATFORM LAYER

- sAVED GAME lOCATIONS
gETTING A HANDLE TO OUR OWN EXECUTABLE FILE
- aSSET LOADGIN PATH
- tHREADING (launch a thread)
- Raw Input( sumpport for multipel keyboards)
/ sleep/timebeginperiod
- Clip cursor () (multi monitor support)
- Fullscreen
- WM_setcursor (control cursor visiblity)
- QueryCancelAutoPlay
- WM_ActivateApp( for when we are not active app_
- Blit speed improvements (Bltblt)
- Hardware accelation (open gl or drect3d))
- GetKeyboard layout (for french keyboards, internalation WASD support)

partial lits
*/


#include "handmade.h"

#include <stdio.h>
#include <windows.h>
#include <malloc.h>
#include <Xinput.h>
#include <dsound.h>
#include "win32_handmade.h"

global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

#pragma region XInput
#pragma region XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
  return (ERROR_DEVICE_NOT_CONNECTED);
}
#pragma endregion

#pragma region XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
  return (ERROR_DEVICE_NOT_CONNECTED);
}
#pragma endregion

global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;

#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

internal void Win32LoadXInput()
{
  HMODULE XInputLibrary = LoadLibraryA(XINPUT_DLL_A);
  if (XInputLibrary)
  {
    XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
  }
}

internal void Win32ProcessKeyboardMessages(game_button_state* Button, bool32 IsDown)
{
  if (Button->EndedDown != IsDown)
  {
    Button->EndedDown = IsDown;
    Button->HalfTransitionCount++;
  }
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state* OldState, game_button_state* NewState, DWORD ButtonBit)
{
  NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
  NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

#pragma endregion

#pragma region GameCode


inline FILETIME Win32GetLastWriteTime(char* Filename)
{
  FILETIME LastWriteTime = {};

  WIN32_FIND_DATAA FindData;
  HANDLE FindHandle = FindFirstFileA(Filename, &FindData);

  if (FindHandle != INVALID_HANDLE_VALUE)
  {
    LastWriteTime = FindData.ftLastWriteTime;
    FindClose(FindHandle);
  }

  return LastWriteTime;
}

internal win32_game_code Win32LoadGameCode(char* SourceDLLName)
{
  win32_game_code Result = {};

  //need to get proper path here
  // automatic determination of when updates are necessary

  char TempDLLName[] = "handmade_temp.dll";

  Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
  CopyFileA(SourceDLLName, TempDLLName, false);
  Result.GameCodeDLL = LoadLibraryA(TempDLLName);

  if (Result.GameCodeDLL)
  {
    Result.UpdateAndRender = (game_update_and_render*)GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
    Result.GetSoundSamples = (game_get_sound_samples*)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");

    Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
  }

  if (!Result.IsValid)
  {
    Result.UpdateAndRender = 0;
    Result.GetSoundSamples = 0;
  }

  return Result;
}

internal void Win32UnloadGameCode(win32_game_code* GameCode)
{
  if (GameCode->GameCodeDLL) {
    FreeLibrary(GameCode->GameCodeDLL);
  }

  GameCode->IsValid = false;
  GameCode->UpdateAndRender = 0;
  GameCode->GetSoundSamples = 0;
}
#pragma endregion

#pragma region DSound
#define DSOUND_DLL "dsound.dll"
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

DIRECT_SOUND_CREATE(DirectSoundCreateStub)
{
  return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable direct_sound_create* DirectSoundCreate_ = DirectSoundCreateStub;
#define DirectSoundCreate DirectSoundCreate_


internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
  //1 loadLibrary Dsound

  HMODULE DSoundLibrary = LoadLibraryA(DSOUND_DLL);
  if (DSoundLibrary)
  {
    DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

    // 2 Get DirectSound object

    LPDIRECTSOUND DirectSound;
    if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
    {
      WAVEFORMATEX WaveFormat = {};
      WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
      WaveFormat.nChannels = 2;
      WaveFormat.nSamplesPerSec = SamplesPerSecond;
      WaveFormat.wBitsPerSample = 16;
      WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8; // bytes per single unit of channel [Left Right] channel 32bits /8 => 4 bytes
      WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
      WaveFormat.cbSize = 0;

      if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
      {
        DSBUFFERDESC BufferDescription = {  };
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

        // 3 Create Primary buffer
        LPDIRECTSOUNDBUFFER PrimaryBuffer;
        if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
        {
          // int16 samples; // waveForm

          if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
          {
            // we have set format of primary buffer
            OutputDebugStringA("Primary buffer succesfully set\n");
          }
          else
          {

            // Logging
          }
        }
        else
        {
          // Logging
        }
      }
      else
      {
        // logging
      }

      // 4 Create a secondary buffer

      DSBUFFERDESC BufferDescription = {  };
      BufferDescription.dwSize = sizeof(BufferDescription);
      BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
      BufferDescription.dwBufferBytes = BufferSize;
      BufferDescription.lpwfxFormat = &WaveFormat;

      HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
      if (SUCCEEDED(Error))
      {
        OutputDebugStringA("Secondary buffer succesfully set\n");
        //// int16 samples; // waveForm

        //if (SUCCEEDED(GlobalSecondaryBuffer->SetFormat(&WaveFormat)))
        //{
        //    // we have set format of primary buffer

        //}
        //else
        //{
        //    // Logging
        //}
      }
      else
      {
        // Logging
      }
    }
    else
    {
      // logging
    }
  }
  else
  {
    //logging
  }

  // 5 Start it playing
}

internal void Win32ClearSoundBuffer(win32_sound_output* SoundOutput)
{

  VOID* Region1;
  DWORD Region1Size;
  VOID* Region2;
  DWORD  Region2Size;

  if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
  {
    uint8* DestSample = (uint8*)Region1;
    for (DWORD ByteIndex = 0; ByteIndex < Region1Size;ByteIndex++)
    {
      *DestSample++ = 0;
    }

    DestSample = (uint8*)Region2;
    for (DWORD ByteIndex = 0; ByteIndex < Region2Size;ByteIndex++)
    {
      *DestSample++ = 0;
    }
    GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }

}

internal void Win32FillSoundBuffer(win32_sound_output* SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer* GameBuffer)
{
  VOID* Region1;
  DWORD Region1Size;
  VOID* Region2;
  DWORD  Region2Size;

  if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
  {
    int16* DestSample = (int16*)Region1;
    int16* SourceSample = GameBuffer->Samples;

    DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
    for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount;SampleIndex++)
    {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      SoundOutput->RunningSampleIndex++;
    }

    DestSample = (int16*)Region2;
    DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
    for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount;SampleIndex++) {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      SoundOutput->RunningSampleIndex++;
    }

    GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }
}
#pragma endregion

internal int StringLength(char* String)
{
  int Count = 0;
  while (*String++) {
    Count++;
  }
  return Count;
}

internal void CatStrings(size_t SourceACount, char* SourceA,
  size_t SourceBCount, char* SourceB,
  size_t DestCount, char* Dest)
{
  // NOTE(casey): DestCount is assumed to NOT include a null terminator!

  size_t Counter = 0;

  // Copy first string
  for (size_t Index = 0;
    (Index < SourceACount) && (Counter < DestCount);
    ++Index, ++Counter)
  {
    *Dest++ = SourceA[Index];
  }

  // Copy second string
  for (size_t Index = 0;
    (Index < SourceBCount) && (Counter < DestCount);
    ++Index, ++Counter)
  {
    *Dest++ = SourceB[Index];
  }

  // Null terminate if there's room
  if (Counter < DestCount)
  {
    *Dest = 0;
  }
}

internal void Win32BuildEXEPathFileName(win32_state* State, char FileName[], int DestCount, char* Dest)
{
  CatStrings(State->OnePastLastEXEFileNameSlash - State->EXEFileName, State->EXEFileName, StringLength(FileName), FileName, DestCount, Dest);
}

#pragma region FileIO
DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
  if (Memory) {
    VirtualFree(Memory, 0, MEM_RELEASE);
  }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
  debug_read_file_result Result = {};

  HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
  if (FileHandle != INVALID_HANDLE_VALUE)
  {
    LARGE_INTEGER FileSize;
    if (GetFileSizeEx(FileHandle, &FileSize))
    {
      uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
      void* Memory = VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

      if (Memory)
      {
        DWORD BytesRead;
        if (ReadFile(FileHandle, Memory, FileSize32, &BytesRead, 0)
          && (FileSize32 == BytesRead))
        {
          Result.ContentsSize = FileSize32;
          Result.Contents = Memory;
        }
        else
        {
          DEBUGPlatformFreeFileMemory(Thread, Memory);
          Memory = 0;
        }
      }
    }

    CloseHandle(FileHandle);
  }

  return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
  bool32  Result = false;

  HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (FileHandle != INVALID_HANDLE_VALUE)
  {
    DWORD BytesWritten;
    if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
    {
      Result = (BytesWritten == MemorySize);
    }

    CloseHandle(FileHandle);
  }

  return Result;
}

#pragma endregion
internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
  RECT ClientRect;
  GetClientRect(Window, &ClientRect);

  win32_window_dimension Result = {};
  int Width = ClientRect.right - ClientRect.left;
  int Height = ClientRect.bottom - ClientRect.top;
  Result.Width = Width;
  Result.Height = Height;

  return(Result);
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer* Buffer, int Width, int Height)
{
  //bulletproof this
  // maybe don't free first, free after, the free first if that fails

  if (Buffer->Memory)
  {
    VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
  }

  int BytesPerPixel = 4;
  Buffer->BytesPerPixel = BytesPerPixel;

  Buffer->Width = Width;
  Buffer->Height = Height;
  Buffer->Pitch = Width * BytesPerPixel;

  Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
  Buffer->Info.bmiHeader.biWidth = Buffer->Width;
  Buffer->Info.bmiHeader.biHeight = -Buffer->Height; // neg makes sure image generates from top to bottom instead of bottom to top
  Buffer->Info.bmiHeader.biPlanes = 1;
  Buffer->Info.bmiHeader.biBitCount = 32;
  Buffer->Info.bmiHeader.biCompression = BI_RGB;

  int BitmapMemorySize = (Width * Height) * BytesPerPixel;

  Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); // returns the requested memory
}

internal void Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth, int WindowHeight, win32_offscreen_buffer* Buffer, int X, int Y, int Width, int Height)
{
  int OffsetX = 10;
  int OffsetY = 10;

  PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS); //top
  PatBlt(DeviceContext, 0, OffsetY + Buffer->Height, WindowWidth, WindowHeight, BLACKNESS); // right
  PatBlt(DeviceContext, OffsetX + Buffer->Width, 0, WindowWidth, WindowHeight, BLACKNESS); // bottom
  PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS); // left

  // for protottype purposes we are going to always blib 1-to-1 pixels 
      //rect to rect copy
  StretchDIBits(DeviceContext,
    // Dest rect
    //X, Y, Width, Height,
    // Src rect
    //X, Y, Width, Height,
    OffsetX, OffsetY, Buffer->Width, Buffer->Height,
    0, 0, Buffer->Width, Buffer->Height,
    Buffer->Memory,
    &Buffer->Info,
    DIB_RGB_COLORS,
    SRCCOPY
  );
}


LRESULT CALLBACK Win32MainWindowCallback(
  HWND Window,
  UINT Message,
  WPARAM WParam,
  LPARAM LParam)
{
  LRESULT Result = 0;

  switch (Message)
  {
    case WM_ACTIVATEAPP:
    {
      OutputDebugStringA("WM_ACTIVATEAPP\n");
      break;
    }
    case WM_CLOSE:
    {
      // Handle with message to user
      GlobalRunning = false;
      break;
    }
    case WM_DESTROY:
    {
      // handle this as an error - recreate window ? 
      GlobalRunning = false;
      break;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
      Assert("!Keyboard input came in through non dispatch event")
        uint32 VKCode = (uint32)WParam;

      bool32 AltKeyWasDown = (LParam & (1 << 29));
      if ((VKCode == VK_F4) && AltKeyWasDown)
      {
        GlobalRunning = false;
      }

      bool32 WasDown = LParam & (1 << 30);
      bool32 IsDown = !(LParam & (1 << 31));

      if (WasDown != IsDown)
      {
        if (VKCode == 'W') {}
        if (VKCode == 'A') {}
        if (VKCode == 'S') {}
        if (VKCode == 'D') {}
        if (VKCode == 'Q') {}
        if (VKCode == 'E') {}
        if (VKCode == VK_ESCAPE) {}
        if (VKCode == VK_SPACE) {}

        if (VKCode == VK_UP)
        {
          OutputDebugStringA("UP\n");
        }

        if (VKCode == VK_DOWN)
        {

        }
        if (VKCode == VK_LEFT)
        {

        }
        if (VKCode == VK_RIGHT)
        {

        }
        break;
      }

    }

    case WM_PAINT:
    {
      PAINTSTRUCT Paint;
      HDC DeviceContext = BeginPaint(Window, &Paint);
      int X = Paint.rcPaint.left;
      int Y = Paint.rcPaint.top;
      int Width = Paint.rcPaint.right - Paint.rcPaint.left;
      int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
      win32_window_dimension Dimension = Win32GetWindowDimension(Window);

      Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer, X, Y, Width, Height);

      EndPaint(Window, &Paint);
      break;
    }

    default:
    {
      Result = DefWindowProcA(Window, Message, WParam, LParam);
      break;
    }
  }

  return Result;
}

internal void Win32GetInputFileLocation(win32_state* State, bool32 InputStream, int SlotIndex, int DestCount, char* Dest)
{
  char FileName[64];
  wsprintfA(FileName, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
  Win32BuildEXEPathFileName(State, FileName, DestCount, Dest);
}

internal win32_replay_buffer* Win32GetReplayBuffer(win32_state* State, int unsigned Index)
{
  Assert(Index < ArrayCount(State->ReplayBuffers));
  win32_replay_buffer* Result = &State->ReplayBuffers[Index];
  return Result;
}

internal void Win32BeginRecordingInput(win32_state* State, int InputRecordingIndex)
{
  win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex);

  if (ReplayBuffer->MemoryBlock)
  {
    State->InputRecordingIndex = InputRecordingIndex;

    char FileName[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(State, true, InputRecordingIndex, sizeof(FileName), FileName);

    State->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

    // LARGE_INTEGER FilePosition;
    // FilePosition.QuadPart = State->TotalSize;
    // if (SetFilePointerEx(State->RecordingHandle, FilePosition, 0, FILE_BEGIN))
    // {
    // }

    CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
  }
}

internal void Win32EndRecordingInput(win32_state* State)
{
  CloseHandle(State->RecordingHandle);
  State->InputRecordingIndex = 0;
}

internal void Win32BeginPlayBackInput(win32_state* State, int InputPlayingIndex)
{
  win32_replay_buffer* ReplayBuffer = Win32GetReplayBuffer(State, InputPlayingIndex);

  if (ReplayBuffer->MemoryBlock)
  {
    State->InputPlayingIndex = InputPlayingIndex;

    char FileName[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(State, true, InputPlayingIndex, sizeof(FileName), FileName);

    State->PlaybackHandle = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

    // LARGE_INTEGER FilePosition;
    // FilePosition.QuadPart = State->TotalSize;
    // if (SetFilePointerEx(State->PlaybackHandle, FilePosition, 0, FILE_BEGIN))
    // {
    // }
    CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
  }
}

internal void Win32EndPlayBackInput(win32_state* State)
{
  CloseHandle(State->PlaybackHandle);
  State->InputPlayingIndex = 0;
}

internal void Win32RecordInput(win32_state* State, game_input* NewInput)
{
  DWORD BytesWritten;
  WriteFile(State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void Win32PlayBackInput(win32_state* State, game_input* NewInput)
{
  DWORD BytesRead;
  if (ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
  {
    // There is still input
    if (BytesRead == 0) {
      // we've ended and now bein starting from 0
      int PlayingIndex = State->InputPlayingIndex;
      Win32EndPlayBackInput(State);
      Win32BeginPlayBackInput(State, PlayingIndex);
      ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
    }
  }
}

internal void Win32ProcessPendingMessages(win32_state* State, game_controller_input* KeyboardController, game_input* Input)
{
  MSG Message;
  while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
  {
    if (Message.message == WM_QUIT)
    {
      GlobalRunning = false;
    }

    switch (Message.message)
    {

      case WM_SYSKEYDOWN:
      case WM_KEYDOWN:
      case WM_SYSKEYUP:
      case WM_KEYUP:
      {
        uint32 VKCode = (uint32)Message.wParam;

        bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
        if ((VKCode == VK_F4) && AltKeyWasDown)
        {
          GlobalRunning = false;
        }


        // we can't use the real value here and need to check with 0 , bcs we are comparing
        bool32 WasDown = (Message.lParam & (1 << 30)) != 0;
        bool32 IsDown = (Message.lParam & (1 << 31)) == 0;

        if (WasDown != IsDown)
        {
          if (VKCode == 'W' || VKCode == VK_UP) {
            Win32ProcessKeyboardMessages(&KeyboardController->MoveUp, IsDown);
          }
          else if (VKCode == 'A' || VKCode == VK_LEFT) {
            Win32ProcessKeyboardMessages(&KeyboardController->MoveLeft, IsDown);
          }
          else if (VKCode == 'S' || VKCode == VK_DOWN) {
            Win32ProcessKeyboardMessages(&KeyboardController->MoveDown, IsDown);
          }
          else if (VKCode == 'D' || VKCode == VK_RIGHT) {
            Win32ProcessKeyboardMessages(&KeyboardController->MoveRight, IsDown);
          }
          else if (VKCode == 'Q' || VKCode == VK_SPACE) {
            Win32ProcessKeyboardMessages(&KeyboardController->LeftShoulder, IsDown);
          }
          else if (VKCode == 'E') {
            Win32ProcessKeyboardMessages(&KeyboardController->RightShoulder, IsDown);
          }
          else if (VKCode == 'P') {
#if HANDMADE_INTERNAL
            if (IsDown)
            {
              GlobalPause = !GlobalPause;
            }
#endif
          }
          else if (VKCode == 'L')
          {
            if (IsDown)
            {
              if (State->InputPlayingIndex == 0)
              {
                if (State->InputRecordingIndex == 0)
                {
                  State->InputRecordingIndex = 1;
                  Win32BeginRecordingInput(State, 1);
                }
                else
                {

                  Win32EndRecordingInput(State);
                  Win32BeginPlayBackInput(State, 1);
                }
              }
              else
              {
                Win32EndPlayBackInput(State);
              }
            }
          }
          else if (VKCode == WM_LBUTTONDOWN)
          {
            Win32ProcessKeyboardMessages(&Input->MouseButtons[0], true);
          }
          else if (VKCode == WM_LBUTTONUP)
          {
            Win32ProcessKeyboardMessages(&Input->MouseButtons[0], false);
          }
          else if (VKCode == WM_MBUTTONDOWN)
          {
            Win32ProcessKeyboardMessages(&Input->MouseButtons[1], true);
          }
          else if (VKCode == WM_MBUTTONUP)
          {
            Win32ProcessKeyboardMessages(&Input->MouseButtons[1], false);
          }
          else if (VKCode == WM_RBUTTONDOWN)
          {
            Win32ProcessKeyboardMessages(&Input->MouseButtons[2], true);
          }
          else if (VKCode == WM_RBUTTONUP)
          {
            Win32ProcessKeyboardMessages(&Input->MouseButtons[2], false);
          }
        }
      } break;

      default:
      {
        TranslateMessage(&Message);
        DispatchMessageA(&Message);
      } break;
    }
  }
}

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadzoneThreshold)
{
  real32 Result = 0;

  if (Value < -DeadzoneThreshold)  // neg here because the value of sThymbly is Neg here
  {
    Result = (real32)(Value + DeadzoneThreshold) / (32768.0f - DeadzoneThreshold);
  }
  else if (Value > DeadzoneThreshold)
  {
    Result = (real32)(Value + DeadzoneThreshold) / (32767.0f - DeadzoneThreshold);;
  }

  return Result;
}

inline LARGE_INTEGER Win32GetWallClock()
{
  LARGE_INTEGER Result;
  QueryPerformanceCounter(&Result);

  return Result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
  real32 Result = (real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCountFrequency;
  return Result;
}

#if 0
internal void Win32DebugDrawVertical(win32_offscreen_buffer* Backbuffer, int X, int Top, int Bottom, uint32 Color)
{
  if (Top <= 0) Top = 0;
  if (Bottom >= Backbuffer->Height) Bottom = Backbuffer->Height;

  if ((X >= 0 && (X < Backbuffer->Width)))
  {
    uint8* Pixel = (uint8*)Backbuffer->Memory + Top * Backbuffer->Pitch + X * Backbuffer->BytesPerPixel;
    for (int Y = Top; Y < Bottom; Y++)
    {
      *(uint32*)Pixel = Color;
      Pixel += Backbuffer->Pitch;
    }
  }
}

internal void Win32DrawSoundBufferMarker(win32_offscreen_buffer* Backbuffer, win32_sound_output* SoundOutput, real32 Coefficient, int PadX, int Top, int Bottom, DWORD Value, uint32 Color)
{
  real32 XReal = Coefficient * (real32)Value;
  int X = PadX + (int)XReal;

  Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);
}

internal void Win32DebugSyncDisplay(win32_offscreen_buffer* Backbuffer, int MarkerCount, win32_debug_time_marker* Markers,
  int CurrentMarkerIndex, win32_sound_output* SoundOutput, real32 TargetSecondsPerFrame)
{
  int PadX = 16;
  int PadY = 16;

  int LineHeight = 64;

  // secondarybuffer size = width we want to draw

  real32 Coefficient = (real32)(Backbuffer->Width - 2 * PadX) / (real32)SoundOutput->SecondaryBufferSize;
  for (int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
  {
    DWORD PlayColor = 0xFFFFFFFF;
    DWORD  WriteColor = 0xFFFF0000;
    DWORD  ExpectedFlipColor = 0xFFFFFF00;
    DWORD  PlayWindowColor = 0xFFFF00FF;

    win32_debug_time_marker* ThisMarker = &Markers[MarkerIndex];
    Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize); // playcursor should always be smaller than the buffer size 
    Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize); // playcursor should always be smaller than the buffer size 
    Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize); // playcursor should always be smaller than the buffer size 
    Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryBufferSize); // playcursor should always be smaller than the buffer size 
    Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize); // playcursor should always be smaller than the buffer size 
    Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize); // playcursor should always be smaller than the buffer size 

    int Top = PadY;
    int Bottom = PadY + LineHeight;

    if (MarkerIndex == CurrentMarkerIndex)
    {
      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      int FirstTop = Top;

      Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, Coefficient, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
      Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, Coefficient, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, Coefficient, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
      Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, Coefficient, PadX, Top, Bottom, ThisMarker->OutputByteCount + ThisMarker->OutputLocation, WriteColor);

      Top += LineHeight + PadY;
      Bottom += LineHeight + PadY;

      Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, Coefficient, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
    }

    Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, Coefficient, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
    Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, Coefficient, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480 * SoundOutput->BytesPerSample, PlayWindowColor);
    Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, Coefficient, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
  }
}

#endif

internal void Win32GetEXEFileName(win32_state* State)
{
  DWORD SizeOfFileName = GetModuleFileNameA(0, State->EXEFileName, sizeof(State->EXEFileName));
  State->OnePastLastEXEFileNameSlash = State->EXEFileName;
  for (CHAR* Scan = State->EXEFileName; *Scan; Scan++)
  {
    if (*Scan == '\\')
    {
      State->OnePastLastEXEFileNameSlash = Scan + 1;
    }
  }
}

int CALLBACK WinMain(HINSTANCE Instance,
  HINSTANCE PrevInstance,
  LPSTR CommandLine,
  int ShowCode)
{
  win32_state State = {};

  Win32GetEXEFileName(&State);
  char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];

  Win32BuildEXEPathFileName(&State, (char*)"handmade.dll", sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);
  Win32BuildEXEPathFileName(&State, (char*)"handmade_temp.dll", sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);
  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

  UINT DesiredShedulerMS = 1;
  bool32 SleepIsGranular = (timeBeginPeriod(DesiredShedulerMS) == TIMERR_NOERROR);

  Win32LoadXInput();

  WNDCLASSA WindowClass = {};

  Win32ResizeDIBSection(&GlobalBackBuffer, 960, 540);

  WindowClass.style = CS_HREDRAW | CS_VREDRAW;
  WindowClass.lpfnWndProc = Win32MainWindowCallback;
  WindowClass.hInstance = Instance;
  // WindowClass.hIcon = ;
  WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  char SourceDLLName[] = "handmade.dll";

  if (RegisterClassA(&WindowClass))
  {
    HWND Window = CreateWindowExA(
      0,
      WindowClass.lpszClassName,
      "Handmade hero",
      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      0,
      0,
      Instance,
      0);

    if (Window)
    {
      int MonitorRefreshHz = 60;

      HDC RefreshDc = GetDC(Window);
      int Win32RefreshRate = GetDeviceCaps(RefreshDc, VREFRESH);
      ReleaseDC(Window, RefreshDc);

      int GameUpdateHz = (MonitorRefreshHz / 2);

      if (Win32RefreshRate > 1)
      {
        MonitorRefreshHz = Win32RefreshRate;
      }

      real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

      win32_sound_output SoundOutput = {};
      SoundOutput.SamplesPerSecond = 48000;
      SoundOutput.RunningSampleIndex = 0;
      SoundOutput.BytesPerSample = sizeof(int16) * 2; //[LEFT Right]
      SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
      SoundOutput.LatencySampleCount = 3 * (SoundOutput.SamplesPerSecond / GameUpdateHz);
      SoundOutput.SafetyBytes = (int)(((real32)SoundOutput.SamplesPerSecond * (real32)SoundOutput.BytesPerSample / GameUpdateHz) / 3.0f);

      GlobalRunning = true;
      int SoundIsPlaying = false;

      Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
      Win32ClearSoundBuffer(&SoundOutput);
      GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      int16* Samples = (int16*)VirtualAlloc(0, 48000 * 2 * sizeof(int16), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); // returns the requested memory

#if HANDMADE_INTERNAL
      LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
      LPVOID BaseAddress = 0;
#endif
      game_memory GameMemory = {};
      GameMemory.PermanentStorageSize = Megabytes(64);
      GameMemory.TransientStorageSize = Gigabytes((uint64)1);
      GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
      GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
      GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

      State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
      State.GameMemoryBlock = (int16*)VirtualAlloc(BaseAddress, State.TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      GameMemory.PermanentStorage = State.GameMemoryBlock;
      GameMemory.TransientStorage = (int8*)GameMemory.PermanentStorage + GameMemory.TransientStorageSize;

      for (int ReplayIndex = 0;ReplayIndex < ArrayCount(State.ReplayBuffers);ReplayIndex++)
      {
        win32_replay_buffer* ReplayBuffer = &State.ReplayBuffers[ReplayIndex];

        Win32GetInputFileLocation(&State, false, ReplayIndex, sizeof(ReplayBuffer->ReplayFileName), ReplayBuffer->ReplayFileName);

        ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->ReplayFileName, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

        LARGE_INTEGER MaxSize;
        MaxSize.QuadPart = State.TotalSize;
        ReplayBuffer->MemoryMap = CreateFileMapping(ReplayBuffer->FileHandle, 0, PAGE_READWRITE, MaxSize.HighPart, MaxSize.LowPart, 0);
        ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, State.TotalSize);
        (int16*)VirtualAlloc(0, (size_t)State.TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

        if (ReplayBuffer->MemoryBlock)
        {

        }
        else
        {
          // Diagnostics
        }
      }

      if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
      {
        game_input Input[2] = {};

        game_input* NewInput = &Input[0];
        game_input* OldInput = &Input[1];

        int DebugTimeMarkerIndex = 0;
        win32_debug_time_marker DebugTimeMarkers[30] = {};
        LARGE_INTEGER LastCounter = Win32GetWallClock();
        LARGE_INTEGER FlipWallClock = Win32GetWallClock();

        bool32 SoundIsValid = false;
        DWORD AudioLatencyBytes = 0;
        real32 AudioLatencySeconds = 0;

        uint64 LastCycleCount = __rdtsc(); // for profiling

        win32_game_code Game = Win32LoadGameCode(SourceDLLName);
        if (SUCCEEDED(GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING))) {

          while (GlobalRunning)
          {
            NewInput->dtForFrame = TargetSecondsPerFrame;

            FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceDLLName);
            if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) > 0) {
              Win32UnloadGameCode(&Game);
              Game = Win32LoadGameCode(SourceDLLName);
            }

            game_controller_input* OldKeyboardController = GetController(OldInput, 0);
            game_controller_input* NewKeyboardController = GetController(NewInput, 0);
            game_controller_input ZeroController = {};
            *NewKeyboardController = ZeroController;
            NewKeyboardController->IsConnected = true;

            for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons);ButtonIndex++)
            {
              NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
            }

            Win32ProcessPendingMessages(&State, NewKeyboardController, NewInput);

            if (!GlobalPause)
            {
              POINT MouseLocation;
              GetCursorPos(&MouseLocation);
              ScreenToClient(Window, &MouseLocation);
              NewInput->MouseX = MouseLocation.x;
              NewInput->MouseY = MouseLocation.y;
              NewInput->MouseZ = 0;
              NewInput->MouseButtons[0].EndedDown = GetKeyState(VK_LBUTTON) & (1 << 15);
              NewInput->MouseButtons[1].EndedDown = GetKeyState(VK_MBUTTON) & (1 << 15);
              NewInput->MouseButtons[2].EndedDown = GetKeyState(VK_RBUTTON) & (1 << 15);
              NewInput->MouseButtons[3].EndedDown = GetKeyState(VK_XBUTTON1) & (1 << 15);
              NewInput->MouseButtons[4].EndedDown = GetKeyState(VK_XBUTTON2) & (1 << 15);

              DWORD MaxCountrollerCount = XUSER_MAX_COUNT;
              DWORD ArrayCount = (DWORD)(ArrayCount(&NewInput->Controllers)) - 1;
              if (MaxCountrollerCount > ArrayCount) // 5 controllers - keyboardcontroller
              {
                MaxCountrollerCount = ArrayCount;
              }

              // should we poll this more frequentyly
              for (DWORD ControllerIndex = 0; ControllerIndex < MaxCountrollerCount; ControllerIndex++)
              {
                DWORD OurControllerIndex = ControllerIndex + 1;
                XINPUT_STATE ControllerState;
                game_controller_input* OldController = GetController(OldInput, OurControllerIndex);
                game_controller_input* NewController = GetController(NewInput, OurControllerIndex);

                if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                {
                  NewController->IsConnected = true;
                  NewController->IsAnalog = OldController->IsAnalog;
                  // controller is plugged in 

                  XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

                  bool32  UP = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                  bool32  Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                  bool32  Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                  bool32  Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                  NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                  NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                  if (NewController->StickAverageX != 0.0f || NewController->StickAverageY != 0.0f)
                  {
                    NewController->IsAnalog = true;
                  }

                  if (UP) {
                    NewController->StickAverageY = 1.0f;
                    NewController->IsAnalog = false;
                  }
                  if (Down) {
                    NewController->StickAverageY = -1.0f;
                    NewController->IsAnalog = false;
                  }
                  if (Left) {
                    NewController->StickAverageX = -1.0f;
                    NewController->IsAnalog = false;
                  }
                  if (Right) {
                    NewController->StickAverageX = 1.0f;
                    NewController->IsAnalog = false;
                  }

                  real32 Threshold = 0.5f;
                  DWORD FakeLeftButton = NewController->StickAverageX < -Threshold ? 1 : 0;
                  DWORD FakeRightButton = NewController->StickAverageX > Threshold ? 1 : 0;
                  DWORD FakeDownButton = NewController->StickAverageY < -Threshold ? 1 : 0;
                  DWORD FakeUpButton = NewController->StickAverageY > Threshold ? 1 : 0;

                  Win32ProcessXInputDigitalButton(FakeLeftButton, &OldController->MoveLeft, &NewController->MoveLeft, XINPUT_GAMEPAD_A);
                  Win32ProcessXInputDigitalButton(FakeRightButton, &OldController->MoveRight, &NewController->MoveRight, XINPUT_GAMEPAD_B);
                  Win32ProcessXInputDigitalButton(FakeDownButton, &OldController->MoveDown, &NewController->MoveDown, XINPUT_GAMEPAD_X);
                  Win32ProcessXInputDigitalButton(FakeUpButton, &OldController->MoveUp, &NewController->MoveUp, XINPUT_GAMEPAD_Y);

                  Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionDown, &NewController->ActionDown, XINPUT_GAMEPAD_A);
                  Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionRight, &NewController->ActionRight, XINPUT_GAMEPAD_B);
                  Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionLeft, &NewController->ActionLeft, XINPUT_GAMEPAD_X);
                  Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionUp, &NewController->ActionUp, XINPUT_GAMEPAD_Y);
                  Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, &NewController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER);
                  Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, &NewController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER);

                  Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start, &NewController->Start, XINPUT_GAMEPAD_START);
                  Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back, &NewController->Back, XINPUT_GAMEPAD_BACK);
                }
                else
                {
                  NewController->IsConnected = false;
                }
              }

              thread_context Thread = {};
              game_offscreen_buffer Buffer = {};
              Buffer.Height = GlobalBackBuffer.Height;
              Buffer.Width = GlobalBackBuffer.Width;
              Buffer.Memory = GlobalBackBuffer.Memory;
              Buffer.Pitch = GlobalBackBuffer.Pitch;
              Buffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;
              if (State.InputRecordingIndex) // recording is on
              {
                Win32RecordInput(&State, NewInput);
              }
              if (State.InputPlayingIndex) // playing the recording
              {
                Win32PlayBackInput(&State, NewInput);
              }
              if (Game.UpdateAndRender)
              {
                Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &Buffer);
              }


              LARGE_INTEGER AudioWallClock = Win32GetWallClock();
              real32 FromBeginToAudioSeconds = (Win32GetSecondsElapsed(FlipWallClock, AudioWallClock));

              DWORD PlayCursor;
              DWORD WriteCursor;
              if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
              {

                /*
                How sound output computation works:

                we define a safety value  that is the nmber of samples
                we think our game update loop may vary by (let's say up to 2ms)

                When we wake up to write audio, we will look and see what the playcursor position is
                , we will forecat ahead where we think the play cursor will be on the next frame boundary.

                We will then look to see if the Write cursor is before that by at least our safety value
                if it is, the we will write up to the next frame boundary from the write cursor and then 1 frame further.
                Giving us perfect audio sync in the case of a audio card that has low enough latency


                if Write cursor is after that safety margin, then we assume we can never syunc the audio perfectly,
                so we will write one frame's
                worth of audio plus some number of guard samples
                (1ms or something determined to be safe, whatever we think the
                variability of our frame computation is)
                */

                if (!SoundIsValid)
                {
                  SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                  SoundIsValid = true;
                }

                DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
                DWORD ExpectedSoundBytesPerFrame = (int)((real32)(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz);

                real32 SecondsLeftUntilFlip = TargetSecondsPerFrame - FromBeginToAudioSeconds;
                DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame);
                DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;
                DWORD SafeWriteCursor = WriteCursor;

                if (SafeWriteCursor < PlayCursor) {
                  SafeWriteCursor += SoundOutput.SecondaryBufferSize; // normalize
                }

                Assert(SafeWriteCursor >= PlayCursor);


                SafeWriteCursor += SoundOutput.SafetyBytes;
                // the safewrite cursor is later or bigger than the last possible byte of current frame
                bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);
                DWORD TargetCursor = 0;

                if (AudioCardIsLowLatency)
                {
                  TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
                }
                else
                {
                  // high latency
                  TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
                }

                TargetCursor = TargetCursor % SoundOutput.SecondaryBufferSize;

                // we want to get a lower latency offset from play cursor when we actually start have sound effects
                DWORD BytesToWrite = 0;
                if (ByteToLock > TargetCursor) {
                  BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
                  BytesToWrite += TargetCursor;
                }
                else {
                  BytesToWrite = TargetCursor - ByteToLock;
                }

                game_sound_output_buffer SoundBuffer = {};
                SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                SoundBuffer.Samples = Samples;
                if (Game.GetSoundSamples)
                {
                  Game.GetSoundSamples(&Thread, &GameMemory, &SoundBuffer);
                }
                Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);

#if 0
                GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
                win32_debug_time_marker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
                Marker->OutputPlayCursor = PlayCursor;
                Marker->OutputWriteCursor = WriteCursor;
                Marker->OutputLocation = ByteToLock;
                Marker->OutputByteCount = BytesToWrite;
                Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;
                DWORD UnWrappedWriteCursor = WriteCursor;
                if (UnWrappedWriteCursor < PlayCursor) {
                  UnWrappedWriteCursor += SoundOutput.SecondaryBufferSize;
                }

                AudioLatencyBytes = UnWrappedWriteCursor - PlayCursor;
                AudioLatencySeconds = (((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) / (real32)SoundOutput.SamplesPerSecond);
                char AudioBuffer[256];
                _snprintf_s(AudioBuffer, sizeof(AudioBuffer), "LPC: %u BTL:%u, TC:%u, BTW:%u -- PC: %u, WC: %u, Delta: %u  (%fs) ----- ", PlayCursor, ByteToLock, TargetCursor, BytesToWrite, PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
                OutputDebugStringA(AudioBuffer);
#endif
              }
              else
              {
                SoundIsValid = false;
              }

              LARGE_INTEGER WorkCounter = Win32GetWallClock();

              real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

              real32 SecondsElapsedForFrame = WorkSecondsElapsed;
              if (SecondsElapsedForFrame < TargetSecondsPerFrame)
              {
                if (SleepIsGranular)
                {
                  DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
                  if (SleepMS > 0)
                  {
                    Sleep(SleepMS);
                  }
                }

                while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                {
                  SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                }
              }
              else
              {
                // we missed framerate 
              }

              LARGE_INTEGER EndCounter = Win32GetWallClock();
              real32 MSPerFrame = (1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter)); // x clocks  / (clocks/ second) *1000 to capture the ms bcs of the diff is too bgi
              LastCounter = EndCounter;


              HDC DeviceContext = GetDC(Window);
              win32_window_dimension Dimension = Win32GetWindowDimension(Window);

              Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer, 0, 0, Dimension.Width, Dimension.Height);
              ReleaseDC(Window, DeviceContext);


              FlipWallClock = Win32GetWallClock();

#if HANDMADE_INTERNAL
              {
                Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
                win32_debug_time_marker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];

                GlobalSecondaryBuffer->GetCurrentPosition(&Marker->FlipPlayCursor, &Marker->FlipWriteCursor);

              }
#endif

              game_input* Temp = NewInput;
              NewInput = OldInput;
              OldInput = Temp;

              int64 EndCycleCount = __rdtsc();
              uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
              LastCycleCount = EndCycleCount;

#if 0
              real32 FPS = 0.0f;// GlobalPerfCountFrequency / CounterElapsed; // (MSPerFrame seconds / 1000) * Frames / second
              int32 MegaCyclesPerFrame = (int32)CyclesElapsed / (1000 * 1000);
              char WriteBuffer[256];
              _snprintf_s(WriteBuffer, sizeof(WriteBuffer), "%fms/frame , %fps, %d processorCycles/frame \n", MSPerFrame, FPS, MegaCyclesPerFrame);
              OutputDebugStringA(WriteBuffer);

#endif
#if HANDMADE_INTERNAL
              DebugTimeMarkerIndex++;

              if (DebugTimeMarkerIndex >= ArrayCount(DebugTimeMarkers))
              {
                DebugTimeMarkerIndex = 0;
              }
#endif
            }
          }
        }
      }
      else
      {
        // logging
      }
    }
    else
    {
      // Logging
    }
  }
  else
  {
    // logging
  }

  return (0);
}
