#ifndef WAYLAND_PONG_H

#define BUF_COUNT 3

struct game_screen_buffer {
  struct wl_buffer *wlbuf;

  void *memory; /* mmap pointer to this buffer's start */
  int busy;     /* 1 if compositor is using it */
  int width, height;
  int pitch;
  int bytesPerPixel;
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

  struct game_screen_buffer buffers[BUF_COUNT];

  struct wl_callback *frame_cb;
};

struct wayland_game_code {

  game_update_and_render *UpdateAndRender;
  game_get_sound_samples *GetSoundSamples;

  bool32 IsValid;
};
#define WAYLAND_PONG_H
#endif // !WAYLAND_PONG_H
