#include <cstdint>
#include <wayland-client-protocol.h>
#ifndef WAYLAND_PONG_H

#define BUF_COUNT 3
#define assert(x)                                                              \
  if (!(x))                                                                    \
    __builtin_trap();

enum pointer_event_mask {
  POINTER_EVENT_ENTER = 1 << 0,
  POINTER_EVENT_LEAVE = 1 << 1,
  POINTER_EVENT_MOTION = 1 << 2,
  POINTER_EVENT_BUTTON = 1 << 3,
  POINTER_EVENT_AXIS = 1 << 4,
  POINTER_EVENT_AXIS_SOURCE = 1 << 5,
  POINTER_EVENT_AXIS_STOP = 1 << 6,
  POINTER_EVENT_AXIS_DISCRETE = 1 << 7,
};

struct pointer_event {
  uint32_t event_mask;
  wl_fixed_t surface_x, surface_y;
  uint32_t button, state;
  uint32_t time;
  uint32_t serial;
  struct {
    bool valid;
    wl_fixed_t value;
    int32_t discrete;
  } axes[2];
  uint32_t axis_source;
};
enum touch_event_mask {
  TOUCH_EVENT_DOWN = 1 << 0,
  TOUCH_EVENT_UP = 1 << 1,
  TOUCH_EVENT_MOTION = 1 << 2,
  TOUCH_EVENT_CANCEL = 1 << 3,
  TOUCH_EVENT_SHAPE = 1 << 4,
  TOUCH_EVENT_ORIENTATION = 1 << 5,
};

struct touch_point {
  bool valid;
  int32_t id;
  uint32_t event_mask;
  wl_fixed_t surface_x, surface_y;
  wl_fixed_t major, minor;
  wl_fixed_t orientation;
};

struct touch_event {
  uint32_t event_mask;
  uint32_t time;
  uint32_t serial;
  struct touch_point points[10];
};

struct wayland_buffer {
  struct wl_buffer *wlbuf;
  void *Memory;

  int Width;
  int Height;
  int Stride;

  bool32 Busy; /* 1 if compositor is using it */
};

struct wayland_state {
  /* Globals */
  struct wl_display *wl_display;
  struct wl_registry *wl_registry;
  struct wl_shm *wl_shm;
  struct wl_compositor *wl_compositor;
  struct xdg_wm_base *xdg_wm_base;
  struct wl_seat *wl_seat;
  /* Objects */
  struct wl_surface *wl_surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;
  struct wl_pointer *wl_pointer;
  struct wl_keyboard *wl_keyboard;
  struct wl_touch *wl_touch;

  int ClientWidth;
  int ClientHeight;

  uint32_t PendingWidth;
  uint32_t PendingHeight;
  bool32 ResizePending;
  bool32 IsRunning;

  struct wayland_buffer buffers[BUF_COUNT];

  struct wl_callback *frame_cb;
  // ADD THIS:
  void *game_memory_block;
  uint64_t game_memory_size;

  void *BuffersBase;
  std::size_t BuffersSize;
  bool32 BuffersMapped;

  float offset;
  uint32_t last_frame;
  bool closed;
  struct pointer_event pointer_event;

  struct xkb_state *xkb_state;
  struct xkb_context *xkb_context;
  struct xkb_keymap *xkb_keymap;
  struct touch_event touch_event;
};

struct wayland_game_code {

  game_update_and_render *UpdateAndRender;
  game_get_sound_samples *GetSoundSamples;

  bool32 IsValid;
};
#define WAYLAND_PONG_H
#endif // !WAYLAND_PONG_H
