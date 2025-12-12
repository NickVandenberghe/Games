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

#include "wayland_pong.h"

static void screen_to_offscreen(game_screen_buffer *src,
                                game_offscreen_buffer *dst) {
  dst->Memory = src->Memory;
  dst->Width = src->Width;
  dst->Height = src->Height;
  dst->Pitch = src->Pitch;
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

/* wl_buffer release - compositor no longer holds a reference to the buffer */
static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
  struct game_screen_buffer *b = (game_screen_buffer *)data;
  b->Busy = 0;
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

static void present_frame(struct wayland_state *state,
                          game_screen_buffer *buffer) {
  wl_surface_attach(state->wl_surface, buffer->wlbuf, 0, 0);
  wl_surface_damage(state->wl_surface, 0, 0, state->client_width,
                    state->client_height);

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
  int stride = state->client_width * 4;
  size_t single_size = (size_t)stride * (size_t)state->client_height;
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
    struct game_screen_buffer *b = &state->buffers[i];
    b->Width = state->client_width;
    b->Height = state->client_height;
    b->Pitch = stride;
    b->Memory = (uint8_t *)map + i * single_size;
    b->wlbuf =
        wl_shm_pool_create_buffer(pool, i * single_size, /* offset */
                                  state->client_width, state->client_height,
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

    state->client_width = state->PendingWidth;
    state->client_height = state->PendingHeight;

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

static game_screen_buffer *acquire_free_buffer(wayland_state *state) {
  for (int i = 0; i < BUF_COUNT; ++i) {
    if (!state->buffers[i].Busy) {
      return &state->buffers[i];
    }
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  struct wayland_state state;

  // sets all bits to 0 in the state object
  // this makes it we have a fresh object and no unexpected behaviour
  memset(&state, 0, sizeof(state));

  state.client_width = 640;
  state.client_height = 640;
  state.PendingHeight = 0;
  state.PendingWidth = 0;
  state.ResizePending = 0;

  if (connect_wayland_display(&state) > 0) {
    return 1;
  }

  if (get_wayland_globals(&state) > 0) {
    return 2;
  }

  if (add_wayland_surfaces(&state) > 0) {
    return 3;
  }

  if (create_wayland_buffers(&state) > 0) {
    return 4;
  }

  struct game_memory game_memory;
  allocate_game_memory(&state, &game_memory);

  game_state *GameState = (game_state *)game_memory.permanentStorage;
  memset(GameState, 0, sizeof(game_state));
  GameState->PlayerA.playerX = 10;
  GameState->PlayerA.playerY = 10;
  GameState->PlayerB.playerX = 610;
  GameState->PlayerB.playerY = 10;

  int monitorRefreshHz = 2;

  int gameUpdateHz = (monitorRefreshHz / 2);
  std::cout << "gameUpdateHz" << gameUpdateHz << '\n';

  real32 target_dt = (real32)1.0f / (real32)gameUpdateHz;
  real32 last_time = get_time_seconds();

  std::cout << "last_time" << last_time << '\n';
  real32 accumulator = 0.0f;

  while (1) {
    // std::cout << "1\n" << std::flush; // Prints dots if loop runs
    int rc = wl_display_dispatch_pending(state.wl_display);
    if (rc == -1)
      break;

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
      update_game(GameState, target_dt);
      accumulator -= target_dt;
    }

    // std::cout << "5\n" << std::flush; // Prints dots if loop runs
    if (state.frame_cb == NULL) {
      // std::cout << "6\n" << std::flush; // Prints dots if loop runs
      game_screen_buffer *free_buffer = acquire_free_buffer(&state);
      // std::cout << "7\n" << std::flush; // Prints dots if loop runs
      if (free_buffer) {

        // std::cout << "8\n" << std::flush; // Prints dots if loop runs
        game_offscreen_buffer GameBuffer = {};
        screen_to_offscreen(free_buffer, &GameBuffer);

        // std::cout << "9\n" << std::flush; // Prints dots if loop runs
        render_frame(GameState, &GameBuffer);
        // std::cout << "10\n" << std::flush; // Prints dots if loop runs
        present_frame(&state, free_buffer);
      }
    }

    // std::cout << "11\n" << std::flush; // Prints dots if loop runs
    rc = wl_display_dispatch(state.wl_display);
    if (rc == -1)
      break;
    // std::cout << "12\n" << std::flush; // Prints dots if loop runs
  }

  wl_display_disconnect(state.wl_display);

  return 0;
}
