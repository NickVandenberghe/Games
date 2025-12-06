#include <cstdint>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <unistd.h>

#include "xdg-shell-client-protocol.h"
#include <iostream>
#include <string.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#define BUF_COUNT 3

struct buffer {
  struct wl_buffer *wlbuf;
  void *data; /* mmap pointer to this buffer's start */
  size_t size;
  int busy; /* 1 if compositor is using it */
  int width, height;
  int stride;
};

/* Wayland code */
struct client_state {
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

  struct buffer buffers[BUF_COUNT];

  struct wl_callback *frame_cb;
};

/* wl_buffer release - compositor no longer holds a reference to the buffer */
static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
  struct buffer *b = (buffer *)data;
  b->busy = 0;
}
static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release};

static void frame_done(void *data, struct wl_callback *cb, uint32_t time) {
  struct client_state *a = (client_state *)data;
  if (a->frame_cb) {
    wl_callback_destroy(a->frame_cb);
    a->frame_cb = NULL;
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
               uint32_t color) {
  for (int y = y0; y < y0 + h; y++) {
    for (int x = x0; x < x0 + w; x++) {
      buf[y * width + x] = color;
    }
  }
}

/* paint into the buffer memory (XRGB8888 assumed) */
static void paint(struct buffer *b, struct client_state *a) {
  std::cout << "started painting" << '\n';
  uint32_t *p = (uint32_t *)b->data;
  int width = b->width;
  int height = b->height;

  std::cout << "before background" << '\n';

  /* fill background */
  uint32_t bg = 0xFF202030; /* dark blue-ish */
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      p[y * width + x] = bg;
    }
  }

  draw_rect(p, width, 0, 0, width, height, bg);

  uint32_t color = 0xFFFFFFFF; // white
  draw_rect(p, width, 20, 20, 30, height - 40, color);
  draw_rect(p, width, 600, 20, 30, height - 40, color);
}

static void draw_frame(struct client_state *state) {
  struct buffer *b = NULL;

  /* find first non-busy buffer */
  for (int i = 0; i < BUF_COUNT; ++i) {
    if (!state->buffers[i].busy) {
      b = &state->buffers[i];
      break;
    }
  }

  if (!b) {
    /* All buffers busy; skip this draw. With triple buffering this is much
     * rarer. */
    return;
  }

  std::cout << " started drawing frame" << '\n';

  paint(b, state);

  std::cout << " painted frame" << '\n';

  wl_surface_attach(state->wl_surface, b->wlbuf, 0, 0);
  wl_surface_damage(state->wl_surface, 0, 0, 640, 480);

  state->frame_cb = wl_surface_frame(state->wl_surface);
  wl_callback_add_listener(state->frame_cb, &frame_listener, state);

  wl_surface_commit(state->wl_surface);
  b->busy = 1;
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

/* create triple shm-backed buffers in one pool */
static int create_triple_shm_buffers(struct client_state *a, int width,
                                     int height) {
  int stride = width * 4;
  size_t single_size = (size_t)stride * (size_t)height;
  size_t pool_size = single_size * BUF_COUNT;

  int fd = allocate_shm_file(pool_size);
  if (fd < 0) {
    fprintf(stderr, "failed to create shm file\n");
    return -1;
  }

  void *map = mmap(NULL, pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED) {
    close(fd);
    fprintf(stderr, "mmap failed\n");
    return -1;
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(a->wl_shm, fd, pool_size);
  if (!pool) {
    munmap(map, pool_size);
    close(fd);
    fprintf(stderr, "wl_shm_create_pool failed\n");
    return -1;
  }

  std::cout << "test 3" << '\n';

  for (int i = 0; i < BUF_COUNT; ++i) {
    struct buffer *b = &a->buffers[i];
    b->width = width;
    b->height = height;
    b->stride = stride;
    b->size = single_size;
    b->data = (uint8_t *)map + i * single_size;
    b->wlbuf = wl_shm_pool_create_buffer(pool, i * single_size, /* offset */
                                         width, height, stride,
                                         WL_SHM_FORMAT_XRGB8888);
    wl_buffer_add_listener(b->wlbuf, &wl_buffer_listener, b);
    b->busy = 0;
  }

  wl_shm_pool_destroy(pool);
  close(fd);
  return 0;
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
  struct client_state *state = (client_state *)data;

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

// surface listeners
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                  uint32_t serial) {
  client_state *state = (client_state *)data;
  xdg_surface_ack_configure(xdg_surface, serial);

  draw_frame(state);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static int connect_wayland_display(client_state *state) {
  // 1. we need to create a display
  state->wl_display = wl_display_connect(NULL);

  if (!state->wl_display) {

    std::cout << "Unable to connect to wayland server" << '\n';
    return 1;
  }

  std::cout << "Created display!" << '\n';
  return 0;
}

static int get_wayland_globals(client_state *state) {
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

static int add_wayland_surfaces(client_state *state) {
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

int main(int argc, char *argv[]) {
  struct client_state state;

  memset(&state, 0, sizeof(state));

  if (connect_wayland_display(&state) > 0) {
    return 1;
  }

  if (get_wayland_globals(&state) > 0) {
    return 2;
  }

  if (add_wayland_surfaces(&state) > 0) {
    return 3;
  }

  /* create triple shm buffers */
  if (create_triple_shm_buffers(&state, 640, 480) < 0) {
    fprintf(stderr, "failed to create buffers\n");
    return 1;
  }

  std::cout << " buffers created" << '\n';

  while (1) {
    int rc = wl_display_dispatch_pending(state.wl_display);
    if (rc == -1)
      break;

    std::cout << "test 1" << '\n';
    /* if we have no pending frame callback and a free buffer, draw */
    bool any_free = false;
    for (int i = 0; i < BUF_COUNT; ++i) {
      if (!state.buffers[i].busy) {
        any_free = true;
        break;
      }
    }

    std::cout << "test 2" << '\n';

    if (any_free && state.frame_cb == NULL) {
      draw_frame(&state);
    }

    std::cout << "test 3" << '\n';
    rc = wl_display_dispatch(state.wl_display);
    if (rc == -1)
      break;
  }

  wl_display_disconnect(state.wl_display);

  return 0;
}
