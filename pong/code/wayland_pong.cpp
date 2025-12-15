#include "pong.h"

#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <unistd.h>

#include "pong_platform.h"
#include "xdg-shell-client-protocol.h"
#include <iostream>
#include <string.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "wayland_pong.h"

static wayland_state WaylandStateInit() {
  wayland_state state = {};
  // sets all bits to 0 in the state object
  // this makes it we have a fresh object and no unexpected behaviour
  memset(&state, 0, sizeof(state));

  state.IsRunning = 1;

  state.ClientWidth = 640;
  state.ClientHeight = 640;
  state.PendingHeight = 0;
  state.PendingWidth = 0;
  state.ResizePending = 0;
  return state;
}

static void GameStateInit(game_state *GameState) {
  memset(GameState, 0, sizeof(game_state));
  GameState->PlayerA.playerX = 10;
  GameState->PlayerA.playerY = 10;
  GameState->PlayerB.playerX = 610;
  GameState->PlayerB.playerY = 10;
  GameState->Ball.playerY = 320;
  GameState->Ball.playerX = 320;
}

static void screen_to_offscreen(wayland_buffer *src,
                                game_offscreen_buffer *dst) {
  dst->Memory = src->Memory;
  dst->Width = src->Width;
  dst->Height = src->Height;
  dst->Pitch = src->Stride;
  dst->BytesPerPixel = 4;
}

static bool all_buffers_free(wayland_state *state) {
  for (int i = 0; i < BUF_COUNT; ++i) {
    if (state->buffers[i].Busy) {
      return false;
    }
  }
  return true;
}

inline void FillButton(game_button_state *Button, bool32 IsDown) {
  if (Button->EndedDown != IsDown) {
    Button->EndedDown = IsDown;
    ++Button->HalfTransitionCount;
  }
}

static game_key TranslateKey(xkb_keysym_t sym) {
  switch (sym) {
  case XKB_KEY_w:
    return Key_W;
  case XKB_KEY_a:
    return Key_A;
  case XKB_KEY_s:
    return Key_S;
  case XKB_KEY_d:
    return Key_D;
  case XKB_KEY_Up:
    return Key_Up;
  case XKB_KEY_Down:
    return Key_Down;
  case XKB_KEY_Left:
    return Key_Left;
  case XKB_KEY_Right:
    return Key_Right;
  case XKB_KEY_q:
    return Key_Q;
  case XKB_KEY_e:
    return Key_Escape;
  default:
    return Key_Count; // invalid
  }
}
inline void WaylandFillKeyboard(game_controller_input *Keyboard,
                                wayland_state *State) {
  FillButton(&Keyboard->MoveUp,
             State->KeyDown[Key_W] || State->KeyDown[Key_Up]);

  FillButton(&Keyboard->MoveDown,
             State->KeyDown[Key_S] || State->KeyDown[Key_Down]);

  FillButton(&Keyboard->MoveLeft,
             State->KeyDown[Key_A] || State->KeyDown[Key_Left]);

  FillButton(&Keyboard->MoveRight,
             State->KeyDown[Key_D] || State->KeyDown[Key_Right]);

  FillButton(&Keyboard->LeftShoulder, State->KeyDown[Key_Q]);

  FillButton(&Keyboard->RightShoulder, State->KeyDown[Key_E]);
}

/* wl_buffer release - compositor no longer holds a reference to the buffer */
static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
  wayland_buffer *buffer = (wayland_buffer *)data;
  buffer->Busy = 0;
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release};

static void frame_done(void *data, struct wl_callback *cb, uint32_t time) {
  struct wayland_state *state = (wayland_state *)data;
  if (state->frame_cb) {
    wl_callback_destroy(state->frame_cb);
    state->frame_cb = NULL;
  }

  (void)time;
}

static const struct wl_callback_listener frame_listener = {.done = frame_done};

static void attach_buffer(struct wayland_state *state, wayland_buffer *buffer) {
  if (buffer->Busy) {
    __builtin_trap();
  }

  wl_surface_attach(state->wl_surface, buffer->wlbuf, 0, 0);
  wl_surface_damage(state->wl_surface, 0, 0, buffer->Width, buffer->Height);

  state->frame_cb = wl_surface_frame(state->wl_surface);
  wl_callback_add_listener(state->frame_cb, &frame_listener, state);

  wl_surface_commit(state->wl_surface);
  buffer->Busy = 1;
}

static void randname(char *buf) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  long r = ts.tv_nsec ^ ((long)ts.tv_sec);
  for (int i = 0; i < 6; ++i) {
    buf[i] = 'A' + (r & 15);
    r >>= 4;
  }
}

/* Create anonymous shm file; uses shm_open with a random name then unlinks it
 */
static int create_shm_file(void) {
  int retries = 100;
  while (retries--) {
    char name[] = "/wl_shm-XXXXXX";
    randname(name + sizeof(name) - 7);
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) {
      shm_unlink(name); /* unlink immediately; the fd remains valid */
      return fd;
    }
    if (errno != EEXIST)
      break;
  }
  return -1;
}

static int allocate_shm_file(size_t size) {
  int fd = create_shm_file();
  if (fd < 0)
    return -1;
  if (ftruncate(fd, (off_t)size) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

// xdg_wm_base ping pong
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                             uint32_t serial) {
  xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer,
                             uint32_t serial, struct wl_surface *surface,
                             wl_fixed_t surface_x, wl_fixed_t surface_y) {
  struct wayland_state *State = (wayland_state *)data;
  State->pointer_event.event_mask |= POINTER_EVENT_ENTER;
  State->pointer_event.serial = serial;
  State->pointer_event.surface_x = surface_x,
  State->pointer_event.surface_y = surface_y;
}

static void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer,
                             uint32_t serial, struct wl_surface *surface) {
  struct wayland_state *State = (wayland_state *)data;
  State->pointer_event.serial = serial;
  State->pointer_event.event_mask |= POINTER_EVENT_LEAVE;
}
static void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer,
                              uint32_t time, wl_fixed_t surface_x,
                              wl_fixed_t surface_y) {
  struct wayland_state *State = (wayland_state *)data;
  State->pointer_event.event_mask |= POINTER_EVENT_MOTION;
  State->pointer_event.time = time;
  State->pointer_event.surface_x = surface_x,
  State->pointer_event.surface_y = surface_y;
}

static void wl_pointer_button(void *data, struct wl_pointer *wl_pointer,
                              uint32_t serial, uint32_t time, uint32_t button,
                              uint32_t state) {
  struct wayland_state *State = (wayland_state *)data;
  State->pointer_event.event_mask |= POINTER_EVENT_BUTTON;
  State->pointer_event.time = time;
  State->pointer_event.serial = serial;
  State->pointer_event.button = button, State->pointer_event.state = state;
}
static void wl_pointer_axis(void *data, struct wl_pointer *wl_pointer,
                            uint32_t time, uint32_t axis, wl_fixed_t value) {
  struct wayland_state *State = (wayland_state *)data;
  State->pointer_event.event_mask |= POINTER_EVENT_AXIS;
  State->pointer_event.time = time;
  State->pointer_event.axes[axis].valid = true;
  State->pointer_event.axes[axis].value = value;
}

static void wl_pointer_axis_source(void *data, struct wl_pointer *wl_pointer,
                                   uint32_t axis_source) {
  struct wayland_state *State = (wayland_state *)data;
  State->pointer_event.event_mask |= POINTER_EVENT_AXIS_SOURCE;
  State->pointer_event.axis_source = axis_source;
}

static void wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer,
                                 uint32_t time, uint32_t axis) {
  struct wayland_state *State = (wayland_state *)data;
  State->pointer_event.time = time;
  State->pointer_event.event_mask |= POINTER_EVENT_AXIS_STOP;
  State->pointer_event.axes[axis].valid = true;
}

static void wl_pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
                                     uint32_t axis, int32_t discrete) {
  struct wayland_state *State = (wayland_state *)data;
  State->pointer_event.event_mask |= POINTER_EVENT_AXIS_DISCRETE;
  State->pointer_event.axes[axis].valid = true;
  State->pointer_event.axes[axis].discrete = discrete;
}
static void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer) {
  struct wayland_state *State = (wayland_state *)data;
  struct pointer_event *event = &State->pointer_event;
  fprintf(stderr, "pointer frame @ %d: ", event->time);

  if (event->event_mask & POINTER_EVENT_ENTER) {
    fprintf(stderr, "entered %f, %f ", wl_fixed_to_double(event->surface_x),
            wl_fixed_to_double(event->surface_y));
  }

  if (event->event_mask & POINTER_EVENT_LEAVE) {
    fprintf(stderr, "leave");
  }

  if (event->event_mask & POINTER_EVENT_MOTION) {
    fprintf(stderr, "motion %f, %f ", wl_fixed_to_double(event->surface_x),
            wl_fixed_to_double(event->surface_y));
  }

  if (event->event_mask & POINTER_EVENT_BUTTON) {
    char *EventState =
        (char *)(event->state == WL_POINTER_BUTTON_STATE_RELEASED ? "released"
                                                                  : "pressed");
    fprintf(stderr, "button %d %s ", event->button, EventState);
  }

  uint32_t axis_events = POINTER_EVENT_AXIS | POINTER_EVENT_AXIS_SOURCE |
                         POINTER_EVENT_AXIS_STOP | POINTER_EVENT_AXIS_DISCRETE;
  char *axis_name[2] = {
      [WL_POINTER_AXIS_VERTICAL_SCROLL] = "vertical",
      [WL_POINTER_AXIS_HORIZONTAL_SCROLL] = "horizontal",
  };
  char *axis_source[4] = {
      [WL_POINTER_AXIS_SOURCE_WHEEL] = "wheel",
      [WL_POINTER_AXIS_SOURCE_FINGER] = "finger",
      [WL_POINTER_AXIS_SOURCE_CONTINUOUS] = "continuous",
      [WL_POINTER_AXIS_SOURCE_WHEEL_TILT] = "wheel tilt",
  };
  if (event->event_mask & axis_events) {
    for (size_t i = 0; i < 2; ++i) {
      if (!event->axes[i].valid) {
        continue;
      }
      fprintf(stderr, "%s axis ", axis_name[i]);
      if (event->event_mask & POINTER_EVENT_AXIS) {
        fprintf(stderr, "value %f ", wl_fixed_to_double(event->axes[i].value));
      }
      if (event->event_mask & POINTER_EVENT_AXIS_DISCRETE) {
        fprintf(stderr, "discrete %d ", event->axes[i].discrete);
      }
      if (event->event_mask & POINTER_EVENT_AXIS_SOURCE) {
        fprintf(stderr, "via %s ", axis_source[event->axis_source]);
      }
      if (event->event_mask & POINTER_EVENT_AXIS_STOP) {
        fprintf(stderr, "(stopped) ");
      }
    }
  }

  fprintf(stderr, "\n");
  memset(event, 0, sizeof(*event));
}

static const struct wl_pointer_listener wl_pointer_listener = {
    .enter = wl_pointer_enter,
    .leave = wl_pointer_leave,
    .motion = wl_pointer_motion,
    .button = wl_pointer_button,
    .axis = wl_pointer_axis,
    .frame = wl_pointer_frame,
    .axis_source = wl_pointer_axis_source,
    .axis_stop = wl_pointer_axis_stop,
    .axis_discrete = wl_pointer_axis_discrete,
};

static void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
                               uint32_t format, int32_t fd, uint32_t size) {
  struct wayland_state *State = (wayland_state *)data;
  assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

  char *map_shm = (char *)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  assert(map_shm != MAP_FAILED);

  struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
      State->xkb_context, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1,
      XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap(map_shm, size);
  close(fd);

  struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);
  xkb_keymap_unref(State->xkb_keymap);
  xkb_state_unref(State->xkb_state);
  State->xkb_keymap = xkb_keymap;
  State->xkb_state = xkb_state;
}
static void wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
                              uint32_t serial, struct wl_surface *surface,
                              struct wl_array *keys) {
  struct wayland_state *State = (wayland_state *)data;
  fprintf(stderr, "keyboard enter; keys pressed are:\n");

  uint32_t *key = static_cast<uint32_t *>(keys->data);
  uint32_t *end = reinterpret_cast<uint32_t *>(static_cast<char *>(keys->data) +
                                               keys->size);

  for (; key < end; ++key) {
    char buf[128];

    xkb_keysym_t sym = xkb_state_key_get_one_sym(State->xkb_state, *key + 8);

    xkb_keysym_get_name(sym, buf, sizeof(buf));
    fprintf(stderr, "sym: %-12s (%d), ", buf, sym);

    xkb_state_key_get_utf8(State->xkb_state, *key + 8, buf, sizeof(buf));

    fprintf(stderr, "utf8: '%s'\n", buf);
  }
}
static void wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
                            uint32_t serial, uint32_t time, uint32_t key,
                            uint32_t state) {
  struct wayland_state *State = (wayland_state *)data;
  char buf[128];
  uint32_t keycode = key + 8;
  xkb_keysym_t sym = xkb_state_key_get_one_sym(State->xkb_state, keycode);

  bool32 IsDown = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
  game_key Key = TranslateKey(sym);
  if (Key < Key_Count) {
    State->KeyDown[Key] = IsDown;
  }
  // xkb_keysym_get_name(sym, buf, sizeof(buf));
  // const char *action =
  //     state == WL_KEYBOARD_KEY_STATE_PRESSED ? "press" : "release";
  // fprintf(stderr, "key %s: sym: %-12s (%d), ", action, buf, sym);
  // xkb_state_key_get_utf8(State->xkb_state, keycode, buf, sizeof(buf));
  // fprintf(stderr, "utf8: '%s'\n", buf);
}

static void wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
                              uint32_t serial, struct wl_surface *surface) {
  fprintf(stderr, "keyboard leave\n");
}

static void wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
                                  uint32_t serial, uint32_t mods_depressed,
                                  uint32_t mods_latched, uint32_t mods_locked,
                                  uint32_t group) {
  struct wayland_state *State = (wayland_state *)data;
  xkb_state_update_mask(State->xkb_state, mods_depressed, mods_latched,
                        mods_locked, 0, 0, group);
}

static void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                    int32_t rate, int32_t delay) {
  /* Left as an exercise for the reader */
}

static const struct wl_keyboard_listener wl_keyboard_listener = {
    .keymap = wl_keyboard_keymap,
    .enter = wl_keyboard_enter,
    .leave = wl_keyboard_leave,
    .key = wl_keyboard_key,
    .modifiers = wl_keyboard_modifiers,
    .repeat_info = wl_keyboard_repeat_info,
};

static void wl_touch_frame(void *data, struct wl_touch *wl_touch) {
  struct wayland_state *State = (wayland_state *)data;
  struct touch_event *touch = &State->touch_event;
  const size_t nmemb = sizeof(touch->points) / sizeof(struct touch_point);
  fprintf(stderr, "touch event @ %d:\n", touch->time);

  for (size_t i = 0; i < nmemb; ++i) {
    struct touch_point *point = &touch->points[i];
    if (!point->valid) {
      continue;
    }
    fprintf(stderr, "point %d: ", touch->points[i].id);

    if (point->event_mask & TOUCH_EVENT_DOWN) {
      fprintf(stderr, "down %f,%f ", wl_fixed_to_double(point->surface_x),
              wl_fixed_to_double(point->surface_y));
    }

    if (point->event_mask & TOUCH_EVENT_UP) {
      fprintf(stderr, "up ");
    }

    if (point->event_mask & TOUCH_EVENT_MOTION) {
      fprintf(stderr, "motion %f,%f ", wl_fixed_to_double(point->surface_x),
              wl_fixed_to_double(point->surface_y));
    }

    if (point->event_mask & TOUCH_EVENT_SHAPE) {
      fprintf(stderr, "shape %fx%f ", wl_fixed_to_double(point->major),
              wl_fixed_to_double(point->minor));
    }

    if (point->event_mask & TOUCH_EVENT_ORIENTATION) {
      fprintf(stderr, "orientation %f ",
              wl_fixed_to_double(point->orientation));
    }

    point->valid = false;
    fprintf(stderr, "\n");
  }
}

static struct touch_point *get_touch_point(struct wayland_state *State,
                                           int32_t id) {
  struct touch_event *touch = &State->touch_event;
  const size_t nmemb = sizeof(touch->points) / sizeof(struct touch_point);
  int invalid = -1;
  for (size_t i = 0; i < nmemb; ++i) {
    if (touch->points[i].id == id) {
      return &touch->points[i];
    }
    if (invalid == -1 && !touch->points[i].valid) {
      invalid = i;
    }
  }
  if (invalid == -1) {
    return NULL;
  }
  touch->points[invalid].valid = true;
  touch->points[invalid].id = id;
  return &touch->points[invalid];
}
static void wl_touch_shape(void *data, struct wl_touch *wl_touch, int32_t id,
                           wl_fixed_t major, wl_fixed_t minor) {
  struct wayland_state *State = (wayland_state *)data;
  struct touch_point *point = get_touch_point(State, id);
  if (point == NULL) {
    return;
  }
  point->event_mask |= TOUCH_EVENT_SHAPE;
  point->major = major, point->minor = minor;
}

static void wl_touch_orientation(void *data, struct wl_touch *wl_touch,
                                 int32_t id, wl_fixed_t orientation) {
  struct wayland_state *State = (wayland_state *)data;
  struct touch_point *point = get_touch_point(State, id);
  if (point == NULL) {
    return;
  }
  point->event_mask |= TOUCH_EVENT_ORIENTATION;
  point->orientation = orientation;
}
static void wl_touch_cancel(void *data, struct wl_touch *wl_touch) {
  struct wayland_state *State = (wayland_state *)data;
  State->touch_event.event_mask |= TOUCH_EVENT_CANCEL;
}
static void wl_touch_motion(void *data, struct wl_touch *wl_touch,
                            uint32_t time, int32_t id, wl_fixed_t x,
                            wl_fixed_t y) {
  struct wayland_state *State = (wayland_state *)data;
  struct touch_point *point = get_touch_point(State, id);
  if (point == NULL) {
    return;
  }
  point->event_mask |= TOUCH_EVENT_MOTION;
  point->surface_x = x, point->surface_y = y;
  State->touch_event.time = time;
}
static void wl_touch_up(void *data, struct wl_touch *wl_touch, uint32_t serial,
                        uint32_t time, int32_t id) {
  struct wayland_state *State = (wayland_state *)data;
  struct touch_point *point = get_touch_point(State, id);
  if (point == NULL) {
    return;
  }
  point->event_mask |= TOUCH_EVENT_UP;
}
static void wl_touch_down(void *data, struct wl_touch *wl_touch,
                          uint32_t serial, uint32_t time,
                          struct wl_surface *surface, int32_t id, wl_fixed_t x,
                          wl_fixed_t y) {
  struct wayland_state *State = (wayland_state *)data;
  struct touch_point *point = get_touch_point(State, id);
  if (point == NULL) {
    return;
  }
  point->event_mask |= TOUCH_EVENT_UP;
  point->surface_x = wl_fixed_to_double(x),
  point->surface_y = wl_fixed_to_double(y);
  State->touch_event.time = time;
  State->touch_event.serial = serial;
}
static const struct wl_touch_listener wl_touch_listener = {
    .down = wl_touch_down,
    .up = wl_touch_up,
    .motion = wl_touch_motion,
    .frame = wl_touch_frame,
    .cancel = wl_touch_cancel,
    .shape = wl_touch_shape,
    .orientation = wl_touch_orientation,
};

static void wl_seat_capabilities(void *data, struct wl_seat *wl_seat,
                                 uint32_t capabilities) {
  struct wayland_state *State = (wayland_state *)data;

  bool have_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;

  if (have_pointer && State->wl_pointer == NULL) {
    State->wl_pointer = wl_seat_get_pointer(State->wl_seat);
    wl_pointer_add_listener(State->wl_pointer, &wl_pointer_listener, State);
  } else if (!have_pointer && State->wl_pointer != NULL) {
    wl_pointer_release(State->wl_pointer);
    State->wl_pointer = NULL;
  }

  bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;

  if (have_keyboard && State->wl_keyboard == NULL) {
    State->wl_keyboard = wl_seat_get_keyboard(State->wl_seat);
    wl_keyboard_add_listener(State->wl_keyboard, &wl_keyboard_listener, State);
  } else if (!have_keyboard && State->wl_keyboard != NULL) {
    wl_keyboard_release(State->wl_keyboard);
    State->wl_keyboard = NULL;
  }
  bool have_touch = capabilities & WL_SEAT_CAPABILITY_TOUCH;

  if (have_touch && State->wl_touch == NULL) {
    State->wl_touch = wl_seat_get_touch(State->wl_seat);
    wl_touch_add_listener(State->wl_touch, &wl_touch_listener, State);
  } else if (!have_touch && State->wl_touch != NULL) {
    wl_touch_release(State->wl_touch);
    State->wl_touch = NULL;
  }
}
static void wl_seat_name(void *data, struct wl_seat *wl_seat,
                         const char *name) {
  fprintf(stderr, "seat name: %s\n", name);
}

static const struct wl_seat_listener wl_seat_listener = {
    .capabilities = wl_seat_capabilities,
    .name = wl_seat_name,
};

// wl_registry_listener
static void registry_global_handler(void *data, struct wl_registry *registry,
                                    uint32_t name, const char *interface,
                                    uint32_t version) {
  struct wayland_state *state = (wayland_state *)data;

  // std::cout << interface << ",version:" << version << '\n';
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    state->wl_compositor = (wl_compositor *)wl_registry_bind(
        registry, name, &wl_compositor_interface, 6);
  }
  if (strcmp(interface, wl_shm_interface.name) == 0) {
    state->wl_shm =
        (wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface, 1);
  }
  if (strcmp(interface, wl_seat_interface.name) == 0) {
    state->wl_seat = (struct wl_seat *)wl_registry_bind(registry, name,
                                                        &wl_seat_interface, 9);
    wl_seat_add_listener(state->wl_seat, &wl_seat_listener, state);
  }
  if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    state->xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(
        registry, name, &xdg_wm_base_interface, 7);
    xdg_wm_base_add_listener(state->xdg_wm_base, &xdg_wm_base_listener, state);
  }
}

struct wl_registry_listener listener = {.global = registry_global_handler};

// buffers
static int destroy_wayland_buffers(wayland_state *state) {
  std::cout << "destroy_wayland_buffers \n";

  // Do NOT attach NULL and commit here — that makes the compositor render
  // an empty surface (visible flicker). Instead, wait until the compositor
  // has released all buffers (wl_buffer.release callbacks cleared Busy).
  if (state->wl_display) {
    wl_display_flush(state->wl_display);
  }

  int safety = 100; // avoid infinite loop
  while (!all_buffers_free(state) && --safety) {
    // roundtrip ensures server processed our requests and sent pending events
    int rc = wl_display_roundtrip(state->wl_display);
    if (rc == -1)
      break;
  }

  if (!all_buffers_free(state)) {
    std::cerr << "Warning: buffers not free before destroying them\n";
    // We continue anyway to avoid deadlock on exit, but this means the
    // compositor may still hold references — be cautious.
  }

  // Destroy the wl_buffer objects locally (this removes our client proxies).
  for (int i = 0; i < BUF_COUNT; ++i) {
    if (state->buffers[i].wlbuf) {
      wl_buffer_destroy(state->buffers[i].wlbuf);
      state->buffers[i].wlbuf = NULL;
    }
  }

  // Unmap the shared memory region if mapped
  if (state->BuffersMapped && state->BuffersBase && state->BuffersSize) {
    munmap(state->BuffersBase, state->BuffersSize);
    state->BuffersBase = NULL;
    state->BuffersSize = 0;
    state->BuffersMapped = false;
  }

  return 0;
}

static int create_wayland_buffers(wayland_state *state) {
  std::cout << "create_wayland_buffers \n";
  int stride = state->ClientWidth * 4;
  size_t single_size = (size_t)stride * (size_t)state->ClientHeight;
  size_t pool_size = single_size * BUF_COUNT;

  int fd = allocate_shm_file(pool_size);
  if (fd < 0) {
    fprintf(stderr, "failed to create shm file\n");
    return 1;
  }

  void *map = mmap(NULL, pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED) {
    close(fd);
    fprintf(stderr, "mmap failed\n");
    return 1;
  }

  // store this so destroy knows what to unmap
  state->BuffersBase = map;
  state->BuffersSize = pool_size;
  state->BuffersMapped = true;

  struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, pool_size);
  if (single_size > SIZE_MAX / BUF_COUNT) {
    fprintf(stderr, "shm pool size overflow\n");
    return 1;
  }
  if (!pool) {
    munmap(map, pool_size);
    close(fd);
    fprintf(stderr, "wl_shm_create_pool failed\n");
    return 1;
  }

  for (int i = 0; i < BUF_COUNT; ++i) {
    wayland_buffer *b = &state->buffers[i];
    b->Width = state->ClientWidth;
    b->Height = state->ClientHeight;
    b->Stride = stride;
    b->Memory = (uint8_t *)map + i * single_size;
    b->wlbuf =
        wl_shm_pool_create_buffer(pool, i * single_size, /* offset */
                                  state->ClientWidth, state->ClientHeight,
                                  stride, WL_SHM_FORMAT_XRGB8888);
    wl_buffer_add_listener(b->wlbuf, &wl_buffer_listener, b);
    b->Busy = 0;
  }

  wl_shm_pool_destroy(pool);
  close(fd);

  return 0;
}
// surface listeners
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                  uint32_t serial) {

  wayland_state *state = (wayland_state *)data;
  xdg_surface_ack_configure(xdg_surface, serial);
  //
  if (state->ResizePending) {
    std::cout << "Applying resize: " << state->PendingWidth << "x"
              << state->PendingHeight << '\n';

    state->ClientWidth = state->PendingWidth;
    state->ClientHeight = state->PendingHeight;

    // Only destroy/recreate if buffers already exist
    if (state->BuffersMapped) {
      destroy_wayland_buffers(state);
      create_wayland_buffers(state);
    }

    state->ResizePending = false;
  }

  // ✅ Clear frame_cb so rendering can start
  state->frame_cb = NULL;

  std::cout << "xdg_surface_configure complete" << '\n';
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_toplevel_handle_configure(void *data,
                                          struct xdg_toplevel *xdg_toplevel,
                                          int32_t width, int32_t height,
                                          struct wl_array *states) {
  wayland_state *state = (wayland_state *)data;
  if (width > 0 && height > 0) {
    state->PendingWidth = width;
    state->PendingHeight = height;
    state->ResizePending = true;
  }
}

static void
xdg_toplevel_handle_configure_bounds(void *data,
                                     struct xdg_toplevel *xdg_toplevel,
                                     int32_t width, int32_t height) {
  std::cout << "configure_bounds";
}

static void xdg_toplevel_handle_close(void *data,
                                      struct xdg_toplevel *xdg_toplevel) {

  std::cout << "close";
}

static void
xdg_toplevel_handle_configure_wm_capabilites(void *data,
                                             struct xdg_toplevel *xdg_toplevel,
                                             struct wl_array *capabilities) {

  std::cout << "wm_capabilities";
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_handle_configure,
    .close = xdg_toplevel_handle_close,
    .configure_bounds = xdg_toplevel_handle_configure_bounds,
    .wm_capabilities = xdg_toplevel_handle_configure_wm_capabilites,
};

static int connect_wayland_display(wayland_state *state) {
  // 1. we need to create a display
  state->wl_display = wl_display_connect(NULL);

  if (!state->wl_display) {

    std::cout << "Unable to connect to wayland server" << '\n';
    return 1;
  }

  std::cout << "Created display!" << '\n';
  return 0;
}

static real32 get_time_seconds() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  return (real32)ts.tv_sec + (real32)ts.tv_nsec / 1e9;
}

static int get_wayland_globals(wayland_state *state) {
  state->wl_registry = wl_display_get_registry(state->wl_display);
  wl_registry_add_listener(state->wl_registry, &listener, state);
  wl_display_roundtrip(state->wl_display);

  if (!state->wl_compositor || !state->wl_shm || !state->xdg_wm_base) {
    fprintf(stderr, "Missing required globals\n");
    return 1;
  }

  std::cout << "Got required globals !" << '\n';
  return 0;
}

static int add_wayland_surfaces(wayland_state *state) {
  // 3. Add surface
  state->wl_surface = wl_compositor_create_surface(state->wl_compositor);
  state->xdg_surface =
      xdg_wm_base_get_xdg_surface(state->xdg_wm_base, state->wl_surface);
  xdg_surface_add_listener(state->xdg_surface, &xdg_surface_listener, state);
  state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface);
  xdg_toplevel_set_title(state->xdg_toplevel, "wayland_pong");
  xdg_toplevel_add_listener(state->xdg_toplevel, &xdg_toplevel_listener, state);
  wl_surface_commit(state->wl_surface);

  std::cout << "Added surface" << '\n';
  return 0;
}

static int connect_wayland_keyboard(wayland_state *State) {
  State->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  return 0;
}

void allocate_game_memory(wayland_state *state, game_memory *memory) {
  uint64 permanent_size = Megabytes(64);
  uint64 transient_size = Megabytes(256);

  uint64 total_size = permanent_size + transient_size;

  state->game_memory_size = total_size;

  state->game_memory_block = mmap(0, total_size, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  memory->permanentStorageSize = permanent_size;
  memory->permanentStorage = state->game_memory_block;

  memory->transientStorageSize = transient_size;
  memory->transientStorage = (uint8 *)state->game_memory_block + permanent_size;
}

static wayland_buffer *acquire_free_buffer(wayland_state *state) {
  for (int i = 0; i < BUF_COUNT; ++i) {
    if (!state->buffers[i].Busy) {
      return &state->buffers[i];
    }
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  wayland_state State = WaylandStateInit();

  if (connect_wayland_display(&State) > 0) {
    return 1;
  }

  if (connect_wayland_keyboard(&State) > 0) {
    return 2;
  }

  if (get_wayland_globals(&State) > 0) {
    return 3;
  }

  if (add_wayland_surfaces(&State) > 0) {
    return 4;
  }

  if (create_wayland_buffers(&State) > 0) {
    return 5;
  }

  struct game_memory game_memory;
  allocate_game_memory(&State, &game_memory);

  game_state *GameState = (game_state *)game_memory.permanentStorage;
  GameStateInit(GameState);
  int monitorRefreshHz = 30;

  int gameUpdateHz = (monitorRefreshHz / 2);
  std::cout << "gameUpdateHz" << gameUpdateHz << '\n';

  real32 target_dt = (real32)1.0f / (real32)gameUpdateHz;
  real32 last_time = get_time_seconds();

  std::cout << "last_time" << last_time << '\n';
  real32 accumulator = 0.0f;

  game_input GameInput[2] = {};

  game_input *NewInput = &GameInput[0];
  game_input *OldInput = &GameInput[1];

  while (State.IsRunning) {
    NewInput->dtForFrame = target_dt;

    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
    game_controller_input ZeroController = {};
    *NewKeyboardController = ZeroController;
    NewKeyboardController->IsConnected = true;

    // std::cout << "1\n" << std::flush; // Prints dots if loop runs
    int rc = wl_display_dispatch_pending(State.wl_display);
    if (rc == -1)
      break;

    WaylandFillKeyboard(NewKeyboardController, &State);

    // std::cout << "2\n" << std::flush; // Prints dots if loop runs
    real32 frame_start = get_time_seconds();
    real32 dt = frame_start - last_time;
    last_time = frame_start;

    // std::cout << "3\n" << std::flush; // Prints dots if loop runs
    // Optional safety clamp (prevents huge dt)
    if (dt > 0.25f)
      dt = 0.25f;

    accumulator += dt;

    // std::cout << "4\n" << std::flush; // Prints dots if loop runs
    while (accumulator >= target_dt) {
      update_game(GameState, NewInput, target_dt);
      accumulator -= target_dt;
    }

    // std::cout << "5\n" << std::flush; // Prints dots if loop runs
    if (State.frame_cb == NULL) {
      // std::cout << "6\n" << std::flush; // Prints dots if loop runs
      wayland_buffer *free_buffer = acquire_free_buffer(&State);
      // std::cout << "7\n" << std::flush; // Prints dots if loop runs
      if (free_buffer) {

        // std::cout << "8\n" << std::flush; // Prints dots if loop runs
        game_offscreen_buffer GameBuffer = {};
        screen_to_offscreen(free_buffer, &GameBuffer);

        // std::cout << "9\n" << std::flush; // Prints dots if loop runs
        render_frame(GameState, NewInput, &GameBuffer);
        // std::cout << "10\n" << std::flush; // Prints dots if loop runs
        attach_buffer(&State, free_buffer);
      }
    }

    game_input *Temp = NewInput;
    NewInput = OldInput;
    OldInput = Temp;

    // std::cout << "11\n" << std::flush; // Prints dots if loop runs
    rc = wl_display_dispatch(State.wl_display);
    if (rc == -1)
      break;
    // std::cout << "12\n" << std::flush; // Prints dots if loop runs
  }

  wl_display_disconnect(State.wl_display);

  return 0;
}
