#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "widget.h"

#define PG_WINDOW_RESIZABLE 1u
#define PG_WINDOW_FRAMELESS 2u
#define PG_WINDOW_BACKGROUND 4u
#define PG_WINDOW_PANEL 8u
#define PG_WINDOW_POPUP 16u
#define PG_WINDOW_NO_MAXIMIZE 32u
#define PG_WINDOW_NO_MINIMIZE 64u

#define PG_TITLEBAR_HEIGHT 34
#define PG_BORDER 1
#define PG_RESIZE_GRIP 6

struct pg_window;

typedef bool (*pg_window_close_fn)(struct pg_window *win, void *data);
typedef bool (*pg_window_key_fn)(struct pg_window *win, const struct pg_event *e, void *data);

struct pg_window *pg_window_new(const char *title, int width, int height, uint32_t flags);
struct pg_window *pg_window_new_at(const char *title, int x, int y, int width, int height, uint32_t flags);
void pg_window_destroy(struct pg_window *win);
void pg_window_close(struct pg_window *win);

void pg_window_set_child(struct pg_window *win, struct pg_widget *child);
struct pg_widget *pg_window_child(const struct pg_window *win);
void pg_window_set_title(struct pg_window *win, const char *title);
const char *pg_window_title(const struct pg_window *win);
void pg_window_set_background(struct pg_window *win, uint32_t color);
void pg_window_set_min_size(struct pg_window *win, int width, int height);
bool pg_window_resize(struct pg_window *win, int width, int height);
void pg_window_size(const struct pg_window *win, int *width, int *height);
struct pg_rect pg_window_content(const struct pg_window *win);
void pg_window_screen_position(const struct pg_window *win, int *x, int *y);

void pg_window_maximize(struct pg_window *win);
void pg_window_restore(struct pg_window *win);
void pg_window_minimize(struct pg_window *win);
void pg_window_fullscreen(struct pg_window *win);
void pg_window_toggle_maximize(struct pg_window *win);
bool pg_window_is_maximized(const struct pg_window *win);
bool pg_window_is_active(const struct pg_window *win);
void pg_window_raise(struct pg_window *win);

void pg_window_on_close(struct pg_window *win, pg_window_close_fn fn, void *data);
void pg_window_on_key(struct pg_window *win, pg_window_key_fn fn, void *data);
void pg_window_set_user(struct pg_window *win, void *user);
void *pg_window_user(const struct pg_window *win);

void pg_window_set_focus(struct pg_window *win, struct pg_widget *widget);
struct pg_widget *pg_window_focus(const struct pg_window *win);
void pg_window_queue_draw(struct pg_window *win);
void pg_window_damage(struct pg_window *win, int x, int y, int width, int height);
void pg_window_flush(struct pg_window *win);
uint32_t *pg_window_pixels(struct pg_window *win, int *stride);
