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

/* wl_buffer release - compositor no longer holds a reference to the buffer */
static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
  struct game_screen_buffer *b = (game_screen_buffer *)data;
  b->busy = 0;
}
static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release};

static void frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
  struct wayland_state *state = (wayland_state *)data;
  if (state->frame_cb)
  {
    wl_callback_destroy(state->frame_cb);
    state->frame_cb = NULL;
  }

  /* update animation state */
  // a->square_x += a->square_dx;
  // a->square_y += a->square_dy;
  // if (a->square_x < 0) {
  //   a->square_x = 0;
  //   a->square_dx = -a->square_dx;
  // }
  // if (a->square_y < 0) {
  //   a->square_y = 0;
  //   a->square_dy = -a->square_dy;
  // }
  // if (a->square_x + a->square_size > a->width) {
  //   a->square_x = a->width - a->square_size;
  //   a->square_dx = -a->square_dx;
  // }
  // if (a->square_y + a->square_size > a->height) {
  //   a->square_y = a->height - a->square_size;
  //   a->square_dy = -a->square_dy;
  // }

  (void)time;
}

static const struct wl_callback_listener frame_listener = {.done = frame_done};

void draw_rect(uint32_t *buf, int width, int x0, int y0, int w, int h,
               uint32_t color)
{
  for (int y = y0; y < y0 + h; y++)
  {
    for (int x = x0; x < x0 + w; x++)
    {
      buf[y * width + x] = color;
    }
  }

  // TEST: new version using pitch
  uint8_t *row = (uint8_t *)buffer->memory + y0 * buffer->pitch + x0 * 4;
  for (int y = 0; y < h; y++)
  {
    uint32_t *pixel = (uint32_t *)row;
    for (int x = 0; x < w; x++)
      pixel[x] = color;

    row += buffer->pitch;
  }
}

static void render_frame(game_state *GameState,
                         struct game_screen_buffer *buffer)
{
  std::cout << "rendering frame \n";
  uint32_t *p = (uint32_t *)buffer->memory;
  int width = buffer->width;
  int height = buffer->height;

  /* fill background */
  uint32_t bg = 0xFF202030; /* dark blue-ish */

  draw_rect(p, width, 0, 0, width, height, bg);

  uint32_t color = 0xFFFFFFFF; // white

  //  player a
  draw_rect(p, width, GameState->PlayerA.playerX, GameState->PlayerA.playerY,
            30, height / 2, color);
  // player b
  draw_rect(p, width, GameState->PlayerB.playerX, GameState->PlayerB.playerY,
            30, height / 2, color);
  // ball
  draw_rect(p, width, 320, 320, 30, 30, color);
}

static void present_frame(struct wayland_state *state,
                          game_screen_buffer *buffer)
{
  std::cout << "presenting frame \n";
  wl_surface_attach(state->wl_surface, buffer->wlbuf, 0, 0);
  wl_surface_damage(state->wl_surface, 0, 0, state->client_width,
                    state->client_height);

  state->frame_cb = wl_surface_frame(state->wl_surface);
  wl_callback_add_listener(state->frame_cb, &frame_listener, state);

  wl_surface_commit(state->wl_surface);
  buffer->busy = 1;
}

static void randname(char *buf)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  long r = ts.tv_nsec ^ ((long)ts.tv_sec);
  for (int i = 0; i < 6; ++i)
  {
    buf[i] = 'A' + (r & 15);
    r >>= 4;
  }
}

/* Create anonymous shm file; uses shm_open with a random name then unlinks it
 */
static int create_shm_file(void)
{
  int retries = 100;
  while (retries--)
  {
    char name[] = "/wl_shm-XXXXXX";
    randname(name + sizeof(name) - 7);
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0)
    {
      shm_unlink(name); /* unlink immediately; the fd remains valid */
      return fd;
    }
    if (errno != EEXIST)
      break;
  }
  return -1;
}

static int allocate_shm_file(size_t size)
{
  int fd = create_shm_file();
  if (fd < 0)
    return -1;
  if (ftruncate(fd, (off_t)size) < 0)
  {
    close(fd);
    return -1;
  }
  return fd;
}

// xdg_wm_base ping pong
static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                             uint32_t serial)
{
  xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

// wl_registry_listener
static void registry_global_handler(void *data, struct wl_registry *registry,
                                    uint32_t name, const char *interface,
                                    uint32_t version)
{
  struct wayland_state *state = (wayland_state *)data;

  if (strcmp(interface, wl_compositor_interface.name) == 0)
  {
    state->wl_compositor = (wl_compositor *)wl_registry_bind(
        registry, name, &wl_compositor_interface, 6);
  }
  if (strcmp(interface, wl_shm_interface.name) == 0)
  {
    state->wl_shm =
        (wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface, 1);
  }
  if (strcmp(interface, xdg_wm_base_interface.name) == 0)
  {
    state->xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(
        registry, name, &xdg_wm_base_interface, 7);
    xdg_wm_base_add_listener(state->xdg_wm_base, &xdg_wm_base_listener, state);
  }
}

struct wl_registry_listener listener = {.global = registry_global_handler};

// surface listeners
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                  uint32_t serial)
{
  wayland_state *state = (wayland_state *)data;
  xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static int connect_wayland_display(wayland_state *state)
{
  // 1. we need to create a display
  state->wl_display = wl_display_connect(NULL);

  if (!state->wl_display)
  {

    std::cout << "Unable to connect to wayland server" << '\n';
    return 1;
  }

  std::cout << "Created display!" << '\n';
  return 0;
}

static real32 get_time_seconds()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  return (real32)ts.tv_sec + (real32)ts.tv_nsec / 1e9;
}

static int get_wayland_globals(wayland_state *state)
{
  state->wl_registry = wl_display_get_registry(state->wl_display);
  wl_registry_add_listener(state->wl_registry, &listener, state);
  wl_display_roundtrip(state->wl_display);

  if (!state->wl_compositor || !state->wl_shm || !state->xdg_wm_base)
  {
    fprintf(stderr, "Missing required globals\n");
    return 1;
  }

  std::cout << "Got required globals !" << '\n';
  return 0;
}

static int add_wayland_surfaces(wayland_state *state)
{
  // 3. Add surface
  state->wl_surface = wl_compositor_create_surface(state->wl_compositor);
  state->xdg_surface =
      xdg_wm_base_get_xdg_surface(state->xdg_wm_base, state->wl_surface);
  xdg_surface_add_listener(state->xdg_surface, &xdg_surface_listener, &state);
  state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface);
  xdg_toplevel_set_title(state->xdg_toplevel, "wayland_pong");
  wl_surface_commit(state->wl_surface);

  std::cout << "Added surface" << '\n';
  return 0;
}

// buffers
static int create_wayland_buffers(wayland_state *state)
{
  std::cout << state->client_width << '\n';
  int stride = state->client_width * 4;
  size_t single_size = (size_t)stride * (size_t)state->client_height;
  size_t pool_size = single_size * BUF_COUNT;

  int fd = allocate_shm_file(pool_size);
  if (fd < 0)
  {
    fprintf(stderr, "failed to create shm file\n");
    return 1;
  }

  void *map = mmap(NULL, pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED)
  {
    close(fd);
    fprintf(stderr, "mmap failed\n");
    return 1;
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, pool_size);
  if (!pool)
  {
    munmap(map, pool_size);
    close(fd);
    fprintf(stderr, "wl_shm_create_pool failed\n");
    return 1;
  }

  for (int i = 0; i < BUF_COUNT; ++i)
  {
    struct game_screen_buffer *b = &state->buffers[i];
    b->width = state->client_width;
    b->height = state->client_height;
    b->pitch = stride;
    b->memory = (uint8_t *)map + i * single_size;
    b->wlbuf =
        wl_shm_pool_create_buffer(pool, i * single_size, /* offset */
                                  state->client_width, state->client_height,
                                  stride, WL_SHM_FORMAT_XRGB8888);
    wl_buffer_add_listener(b->wlbuf, &wl_buffer_listener, b);
    b->busy = 0;
  }

  wl_shm_pool_destroy(pool);
  close(fd);

  return 0;
}
void allocate_game_memory(wayland_state *state, game_memory *memory)
{
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

static void update_game(game_state *GameState, real32 deltaTime)
{
  int speed = 200; // pixels per second

  GameState->PlayerA.playerY += (int)(speed * deltaTime);
  GameState->PlayerB.playerY += (int)(speed * deltaTime);
}

static game_screen_buffer *acquire_free_buffer(wayland_state *state)
{
  for (int i = 0; i < BUF_COUNT; ++i)
  {
    if (!state->buffers[i].busy)
    {
      return &state->buffers[i];
    }
  }
  return NULL;
}

int main(int argc, char *argv[])
{
  struct wayland_state state;

  // sets all bits to 0 in the state object
  // this makes it we have a fresh object and no unexpected behaviour
  memset(&state, 0, sizeof(state));

  state.client_width = 640;
  state.client_height = 640;

  if (connect_wayland_display(&state) > 0)
  {
    return 1;
  }

  if (get_wayland_globals(&state) > 0)
  {
    return 2;
  }

  if (add_wayland_surfaces(&state) > 0)
  {
    return 3;
  }

  if (create_wayland_buffers(&state) > 0)
  {
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

  int monitorRefreshHz = 240;

  int gameUpdateHz = (monitorRefreshHz / 2);

  real32 target_dt = 1.0f / (real32)gameUpdateHz;
  real32 last_time = get_time_seconds();

  std::cout << "last_time" << last_time << '\n';
  real32 accumulator = 0.0f;

  while (1)
  {
    int rc = wl_display_dispatch_pending(state.wl_display);
    if (rc == -1)
      break;

    real32 frame_start = get_time_seconds();
    real32 dt = frame_start - last_time;
    last_time = frame_start;

    // Optional safety clamp (prevents huge dt)
    if (dt > 0.25f)
      dt = 0.25f;

    accumulator += dt;

    while (accumulator >= target_dt)
    {
      update_game(GameState, target_dt);
      accumulator -= target_dt;
    }

    if (state.frame_cb == NULL)
    {
      game_screen_buffer *free_buffer = acquire_free_buffer(&state);
      if (free_buffer)
      {

        render_frame(GameState, free_buffer);
        present_frame(&state, free_buffer);
      }
    }

    rc = wl_display_dispatch(state.wl_display);
    if (rc == -1)
      break;
  }

  wl_display_disconnect(state.wl_display);

  return 0;
}
