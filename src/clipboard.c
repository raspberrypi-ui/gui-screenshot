#include "clipboard.h"
#include "wlr-data-control-unstable-v1-client-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <wayland-client.h>

static const char *g_filepath;
static struct wl_seat *g_seat;
static struct zwlr_data_control_manager_v1 *g_manager;
static struct wl_display *g_display;
static int g_running = 1;

/* ── data source events ─────────────────────────────────────────── */

static void source_send(void *data,
                        struct zwlr_data_control_source_v1 *source,
                        const char *mime_type, int32_t fd)
{
    (void)data;
    (void)source;
    (void)mime_type;

    int src = open(g_filepath, O_RDONLY);
    if (src >= 0) {
        char buf[65536];
        ssize_t n;
        while ((n = read(src, buf, sizeof buf)) > 0) {
            const char *p = buf;
            ssize_t left = n;
            while (left > 0) {
                ssize_t w = write(fd, p, left);
                if (w < 0) {
                    if (errno == EINTR)
                        continue;
                    break;
                }
                p += w;
                left -= w;
            }
        }
        close(src);
    }
    close(fd);
}

static void source_cancelled(void *data,
                              struct zwlr_data_control_source_v1 *source)
{
    (void)data;
    zwlr_data_control_source_v1_destroy(source);
    g_running = 0;
}

static const struct zwlr_data_control_source_v1_listener source_listener = {
    .send = source_send,
    .cancelled = source_cancelled,
};

/* ── registry ───────────────────────────────────────────────────── */

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version)
{
    (void)data;
    if (strcmp(interface, wl_seat_interface.name) == 0) {
        g_seat = wl_registry_bind(registry, name,
                                  &wl_seat_interface, 1);
    } else if (strcmp(interface,
                      zwlr_data_control_manager_v1_interface.name) == 0) {
        g_manager = wl_registry_bind(registry, name,
                                     &zwlr_data_control_manager_v1_interface,
                                     version < 2 ? version : 2);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

/* ── child process: clipboard server ────────────────────────────── */

static void clipboard_child(const char *filepath)
{
    g_filepath = filepath;

    g_display = wl_display_connect(NULL);
    if (!g_display)
        _exit(1);

    struct wl_registry *registry = wl_display_get_registry(g_display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(g_display);

    if (!g_seat || !g_manager) {
        fprintf(stderr, "rpd-screenshot: missing seat or "
                "wlr-data-control-manager\n");
        _exit(1);
    }

    struct zwlr_data_control_source_v1 *source =
        zwlr_data_control_manager_v1_create_data_source(g_manager);
    zwlr_data_control_source_v1_add_listener(source, &source_listener, NULL);
    zwlr_data_control_source_v1_offer(source, "image/png");

    struct zwlr_data_control_device_v1 *device =
        zwlr_data_control_manager_v1_get_data_device(g_manager, g_seat);
    zwlr_data_control_device_v1_set_selection(device, source);

    while (g_running && wl_display_dispatch(g_display) != -1)
        ;

    zwlr_data_control_device_v1_destroy(device);
    wl_registry_destroy(registry);
    wl_display_disconnect(g_display);
    _exit(0);
}

/* ── public API ──────────────────────────────────────────────────── */

int clipboard_serve_file(const char *filepath)
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid > 0)
        return 0;              /* parent returns immediately */

    /* child */
    setsid();
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    clipboard_child(filepath);
    _exit(0);                  /* unreachable */
}

int main (int argc, char *argv[])
{
    clipboard_serve_file (argv[1]);
}
