#include <cstdint>
#ifndef WAYLAND_PONG_H

#define BUF_COUNT 3

struct game_screen_buffer {
  void *Memory; /* mmap pointer to this buffer's start */
  int Busy;     /* 1 if compositor is using it */
  int Width, Height;
  int Pitch;
  int BytesPerPixel;

  struct wl_buffer *wlbuf;
};

struct wayland_state {
  /* Globals */
  struct wl_display *wl_display;
  struct wl_registry *wl_registry;
  struct wl_shm *wl_shm;
  struct wl_compositor *wl_compositor;
  struct xdg_wm_base *xdg_wm_base;
  /* Objects */
  struct wl_surface *wl_surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;

  int client_width;
  int client_height;

  uint32_t PendingWidth;
  uint32_t PendingHeight;
  bool32 ResizePending;

  struct game_screen_buffer buffers[BUF_COUNT];

  struct wl_callback *frame_cb;
  // ADD THIS:
  void *game_memory_block;
  uint64_t game_memory_size;

  void *BuffersBase;
  std::size_t BuffersSize;
  bool32 BuffersMapped;
};

struct wayland_game_code {

  game_update_and_render *UpdateAndRender;
  game_get_sound_samples *GetSoundSamples;

  bool32 IsValid;
};
#define WAYLAND_PONG_H
#endif // !WAYLAND_PONG_H
