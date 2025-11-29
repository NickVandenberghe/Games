#include <cstdint>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <unistd.h>

#include "xdg-shell-client-protocol.h"
#include <iostream>
#include <string.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

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
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                             uint32_t serial);
static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer);

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                  uint32_t serial);

static void registry_global_handler(void *data, struct wl_registry *registry,
                                    uint32_t name, const char *interface,
                                    uint32_t version);
static void registry_global_remove_handler(void *data,
                                           struct wl_registry *registry,
                                           uint32_t name);
static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};

struct wl_registry_listener listener = {.global = registry_global_handler,
                                        .global_remove =
                                            registry_global_remove_handler};
static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static int create_shm_file(size_t size) {

  // int fd = syscall(SYS_memfd_create, "wayland-shm", 0);
  int fd = shm_open("wayland-shm", O_RDWR | O_CREAT | O_EXCL, 0600);
  if (fd < 0)
    shm_unlink("wayland-shm");
  return -1;

  if (ftruncate(fd, size) < 0) {
    close(fd);
    return -1;
  }

  return fd;
}

static struct wl_buffer *draw_frame(struct client_state *state) {
  const int width = 640, height = 480;
  int stride = width * 4;
  int size = stride * height;

  int fd = create_shm_file(size);
  if (fd == -1) {
    return NULL;
  }

  uint32_t *data =
      (uint32_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    close(fd);
    return NULL;
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
  struct wl_buffer *buffer = wl_shm_pool_create_buffer(
      pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);

  /* Draw checkerboxed background */
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if ((x + y / 8 * 8) % 16 < 8)
        data[y * width + x] = 0xFF666666;
      else
        data[y * width + x] = 0xFFEEEEEE;
    }
  }

  munmap(data, size);
  wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
  return buffer;
}

int main(int argc, char *argv[]) {
  struct client_state state = {0};
  // 1. we need to create a display
  state.wl_display = wl_display_connect(NULL);

  if (!state.wl_display) {

    std::cout << "Unable to connect to wayland server" << '\n';
    return 1;
  }

  std::cout << "Created display!" << '\n';

  // 2. get registers from wayland
  state.wl_registry = wl_display_get_registry(state.wl_display);
  wl_registry_add_listener(state.wl_registry, &listener, &state);
  wl_display_roundtrip(state.wl_display);
  std::cout << "get Listener to wayland server register !" << '\n';

  // 3. Add surface
  state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
  state.xdg_surface =
      xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.wl_surface);
  xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
  state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
  xdg_toplevel_set_title(state.xdg_toplevel, "test");

  while (1) {
    wl_display_dispatch(state.wl_display);
  }

  wl_display_disconnect(state.wl_display);

  return 0;
}

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                             uint32_t serial) {
  xdg_wm_base_pong(xdg_wm_base, serial);
}

static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
  /* Sent by the compositor when it's no longer using this buffer */
  wl_buffer_destroy(wl_buffer);
}

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                  uint32_t serial) {
  client_state *state = (client_state *)data;
  xdg_surface_ack_configure(xdg_surface, serial);

  struct wl_buffer *buffer = draw_frame(state);
  wl_surface_attach(state->wl_surface, buffer, 0, 0);
  wl_surface_commit(state->wl_surface);
}

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

static void registry_global_remove_handler(void *data,
                                           struct wl_registry *registry,
                                           uint32_t name) {
  printf("removed: %u\n", name);
}
