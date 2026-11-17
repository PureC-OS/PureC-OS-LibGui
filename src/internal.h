#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pg/pg.h"
#include "vb.h"

struct pg_window {
    struct vb_window *vb;
    struct pg_window *next;
    struct pg_widget *child;
    uint32_t flags;
    int role;
    char title[VB_TEXT_MAX];
    int width, height;
    int min_w, min_h;
    int state;
    bool active;
    bool has_background;
    uint32_t background;
    struct pg_canvas canvas;
    struct pg_rect dirty;
    bool dirty_any;
    bool layout_needed;
    struct pg_widget *hover, *grab, *focus;
    bool focus_visible;
    int title_hot, title_pressed;
    uint64_t title_click_ms;
    struct pg_widget *click_widget;
    uint64_t click_ms;
    int click_count;
    uint32_t buttons;
    int pointer_x, pointer_y;
    bool pointer_inside;
    int cursor;
    pg_window_close_fn on_close;
    void *close_data;
    pg_window_key_fn on_key;
    void *key_data;
    void *user;
    bool dead;
    struct pg_window *owner;
    void (*popup_done)(struct pg_window *win, void *data);
    void *popup_data;
};

static inline int pg__min(int a, int b) { return a < b ? a : b; }
static inline int pg__max(int a, int b) { return a > b ? a : b; }
static inline int pg__clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static inline bool pg__rect_has(struct pg_rect r, int x, int y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static inline bool pg__rect_overlap(struct pg_rect a, struct pg_rect b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

char *pg__strdup(const char *s);
void pg__set_text(char **slot, const char *s);

struct pg_window *pg__windows(void);
struct pg_window *pg__window_of(struct vb_window *vb);
void pg__window_event(struct pg_window *win, const struct vb_event *e);
void pg__flush_all(void);
void pg__reap(void);
bool pg__have_windows(void);
void pg__relayout_all(void);
void pg__window_forget(struct pg_window *win, struct pg_widget *w);
void pg__widget_set_window(struct pg_widget *w, struct pg_window *win);
void pg__draw_widget(struct pg_widget *w, struct pg_canvas *c);
struct pg_widget *pg__hit(struct pg_widget *w, int x, int y);

struct pg_window *pg__popup_new(struct pg_window *owner, int x, int y, int width, int height,
                                void (*done)(struct pg_window *win, void *data), void *data);
void pg__theme_init(void);
bool pg__theme_poll(void *data);

const char *pg__dropdown_label(const char *const *labels, int count, int index);
