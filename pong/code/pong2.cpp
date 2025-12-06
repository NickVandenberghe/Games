/* client_triple.c
 *
 * Minimal Wayland xdg-shell client with wl_shm triple-buffering.
 *
 * Compile with:
 *   wayland-scanner ... (see instructions above)
 *   cc -o game_triple client_triple.c xdg-shell-protocol.c -lwayland-client
 * -lrt -lm
 *
 * Very small, focused example for triple buffering.
 */

#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "xdg-shell-client-protocol.h"
#include <wayland-client.h>

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

/* --- buffer & app structures --- */
#define BUF_COUNT 3

struct buffer {
  struct wl_buffer *wlbuf;
  void *data; /* mmap pointer to this buffer's start */
  size_t size;
  int busy; /* 1 if compositor is using it */
  int width, height;
  int stride;
};

struct app {
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct xdg_wm_base *wm_base;

  struct wl_surface *surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;

  int width, height;

  /* triple buffers */
  struct buffer buffers[BUF_COUNT];

  /* frame callback */
  struct wl_callback *frame_cb;

  /* animation state */
  float square_x, square_y;
  float square_dx, square_dy;
  int square_size;
};

/* wl_buffer release - compositor no longer holds a reference to the buffer */
static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
  struct buffer *b = data;
  b->busy = 0;
}
static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release};

/* frame callback - called when compositor is ready for the next frame */
static void frame_done(void *data, struct wl_callback *cb, uint32_t time) {
  struct app *a = data;
  if (a->frame_cb) {
    wl_callback_destroy(a->frame_cb);
    a->frame_cb = NULL;
  }

  /* update animation state */
  a->square_x += a->square_dx;
  a->square_y += a->square_dy;
  if (a->square_x < 0) {
    a->square_x = 0;
    a->square_dx = -a->square_dx;
  }
  if (a->square_y < 0) {
    a->square_y = 0;
    a->square_dy = -a->square_dy;
  }
  if (a->square_x + a->square_size > a->width) {
    a->square_x = a->width - a->square_size;
    a->square_dx = -a->square_dx;
  }
  if (a->square_y + a->square_size > a->height) {
    a->square_y = a->height - a->square_size;
    a->square_dy = -a->square_dy;
  }

  (void)time;
}
static const struct wl_callback_listener frame_listener = {.done = frame_done};

/* paint into the buffer memory (XRGB8888 assumed) */
static void paint(struct buffer *b, struct app *a) {
  uint32_t *p = (uint32_t *)b->data;
  int width = b->width;
  int height = b->height;

  /* fill background */
  uint32_t bg = 0xFF202030; /* dark blue-ish */
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      p[y * width + x] = bg;
    }
  }

  /* draw square */
  int sx = (int)roundf(a->square_x);
  int sy = (int)roundf(a->square_y);
  int ss = a->square_size;
  uint32_t color = 0xFFFFFF44; /* bright yellow-ish */

  for (int y = sy; y < sy + ss; ++y) {
    if (y < 0 || y >= height)
      continue;
    for (int x = sx; x < sx + ss; ++x) {
      if (x < 0 || x >= width)
        continue;
      p[y * width + x] = color;
    }
  }
}

/* draw next frame: pick an available buffer among BUF_COUNT */
static void draw_frame(struct app *a) {
  struct buffer *b = NULL;

  /* find first non-busy buffer */
  for (int i = 0; i < BUF_COUNT; ++i) {
    if (!a->buffers[i].busy) {
      b = &a->buffers[i];
      break;
    }
  }
  if (!b) {
    /* All buffers busy; skip this draw. With triple buffering this is much
     * rarer. */
    return;
  }

  paint(b, a);

  wl_surface_attach(a->surface, b->wlbuf, 0, 0);
  wl_surface_damage(a->surface, 0, 0, a->width, a->height);

  /* request frame callback so we get notified when it's a good time to draw
   * next */
  a->frame_cb = wl_surface_frame(a->surface);
  wl_callback_add_listener(a->frame_cb, &frame_listener, a);

  wl_surface_commit(a->surface);
  b->busy = 1;
}

/* xdg_surface.configure handler: ack and draw */
static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                  uint32_t serial) {
  struct app *a = data;
  xdg_surface_ack_configure(xdg_surface, serial);
  draw_frame(a);
}
static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base,
                             uint32_t serial) {
  xdg_wm_base_pong(wm_base, serial);
}
static const struct xdg_wm_base_listener wm_base_listener = {
    .ping = xdg_wm_base_ping};

/* registry: bind wl_shm, wl_compositor, xdg_wm_base */
static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version) {
  struct app *a = data;
  if (strcmp(interface, wl_shm_interface.name) == 0) {
    a->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
    a->compositor =
        wl_registry_bind(registry, name, &wl_compositor_interface, 4);
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    a->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(a->wm_base, &wm_base_listener, a);
  }
}
static const struct wl_registry_listener registry_listener = {
    .global = registry_global, .global_remove = NULL};

/* create triple shm-backed buffers in one pool */
static int create_triple_shm_buffers(struct app *a, int width, int height) {
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

  struct wl_shm_pool *pool = wl_shm_create_pool(a->shm, fd, pool_size);
  if (!pool) {
    munmap(map, pool_size);
    close(fd);
    fprintf(stderr, "wl_shm_create_pool failed\n");
    return -1;
  }

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

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  struct app app;
  memset(&app, 0, sizeof(app));
  app.width = 640;
  app.height = 480;

  /* animation */
  app.square_size = 48;
  app.square_x = 20.0f;
  app.square_y = 20.0f;
  app.square_dx = 2.75f;
  app.square_dy = 2.0f;

  app.display = wl_display_connect(NULL);
  if (!app.display) {
    fprintf(stderr, "Failed to connect to Wayland display\n");
    return 1;
  }

  app.registry = wl_display_get_registry(app.display);
  wl_registry_add_listener(app.registry, &registry_listener, &app);
  wl_display_roundtrip(app.display); /* get globals */

  if (!app.compositor || !app.shm || !app.wm_base) {
    fprintf(stderr, "Missing required globals\n");
    return 1;
  }

  /* create surface + xdg surface/toplevel */
  app.surface = wl_compositor_create_surface(app.compositor);
  app.xdg_surface = xdg_wm_base_get_xdg_surface(app.wm_base, app.surface);
  xdg_surface_add_listener(app.xdg_surface, &xdg_surface_listener, &app);
  app.xdg_toplevel = xdg_surface_get_toplevel(app.xdg_surface);
  xdg_toplevel_set_title(app.xdg_toplevel, "Wayland triple-buffer game");
  wl_surface_commit(app.surface);

  /* create triple shm buffers */
  if (create_triple_shm_buffers(&app, app.width, app.height) < 0) {
    fprintf(stderr, "failed to create buffers\n");
    return 1;
  }

  /* main loop: dispatch events and draw when possible */
  while (true) {
    /* process pending events first (non-blocking) */
    int rc = wl_display_dispatch_pending(app.display);
    if (rc == -1)
      break;

    /* if we have no pending frame callback and a free buffer, draw */
    bool any_free = false;
    for (int i = 0; i < BUF_COUNT; ++i) {
      if (!app.buffers[i].busy) {
        any_free = true;
        break;
      }
    }
    if (any_free && app.frame_cb == NULL) {
      draw_frame(&app);
    }

    /* block until next event */
    rc = wl_display_dispatch(app.display);
    if (rc == -1)
      break;
  }

  return 0;
}
