#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "canvas.h"
#include "theme.h"

struct pg_window;
struct pg_widget;

enum pg_align {
    PG_ALIGN_FILL = 0,
    PG_ALIGN_START,
    PG_ALIGN_CENTER,
    PG_ALIGN_END,
};

enum pg_orientation {
    PG_HORIZONTAL = 0,
    PG_VERTICAL,
};

enum pg_signal {
    PG_SIGNAL_CLICKED = 0,
    PG_SIGNAL_CHANGED,
    PG_SIGNAL_ACTIVATE,
    PG_SIGNAL_DESTROY,
    PG_SIGNAL_COUNT,
};

enum pg_event_type {
    PG_EVENT_POINTER_MOVE = 1,
    PG_EVENT_POINTER_DOWN,
    PG_EVENT_POINTER_UP,
    PG_EVENT_WHEEL,
    PG_EVENT_ENTER,
    PG_EVENT_LEAVE,
    PG_EVENT_KEY_DOWN,
    PG_EVENT_KEY_UP,
    PG_EVENT_FOCUS_IN,
    PG_EVENT_FOCUS_OUT,
};

#define PG_BUTTON_LEFT 1u
#define PG_BUTTON_RIGHT 2u
#define PG_BUTTON_MIDDLE 4u

#define PG_MOD_SHIFT 0x11u
#define PG_MOD_CTRL 0x22u
#define PG_MOD_ALT 0x44u
#define PG_MOD_SUPER 0x88u

#define PG_KEY_A 0x04
#define PG_KEY_C 0x06
#define PG_KEY_Q 0x14
#define PG_KEY_V 0x19
#define PG_KEY_W 0x1A
#define PG_KEY_X 0x1B
#define PG_KEY_ENTER 0x28
#define PG_KEY_ESCAPE 0x29
#define PG_KEY_BACKSPACE 0x2A
#define PG_KEY_TAB 0x2B
#define PG_KEY_SPACE 0x2C
#define PG_KEY_F1 0x3A
#define PG_KEY_F12 0x45
#define PG_KEY_HOME 0x4A
#define PG_KEY_PAGE_UP 0x4B
#define PG_KEY_DELETE 0x4C
#define PG_KEY_END 0x4D
#define PG_KEY_PAGE_DOWN 0x4E
#define PG_KEY_RIGHT 0x4F
#define PG_KEY_LEFT 0x50
#define PG_KEY_DOWN 0x51
#define PG_KEY_UP 0x52
#define PG_KEY_KP_ENTER 0x58

struct pg_event {
    enum pg_event_type type;
    int x, y;
    uint32_t button;
    uint32_t buttons;
    int wheel;
    int key;
    uint32_t ch;
    uint8_t mods;
    bool repeat;
    int clicks;
};

typedef void (*pg_callback)(struct pg_widget *widget, void *data);

struct pg_widget_class {
    const char *name;
    size_t size;
    void (*measure)(struct pg_widget *w, int *width, int *height);
    int (*height_for_width)(struct pg_widget *w, int width);
    void (*arrange)(struct pg_widget *w);
    void (*draw)(struct pg_widget *w, struct pg_canvas *c);
    void (*draw_after)(struct pg_widget *w, struct pg_canvas *c);
    bool (*event)(struct pg_widget *w, const struct pg_event *e);
    void (*destroy)(struct pg_widget *w);
};

struct pg_slot {
    pg_callback fn;
    void *data;
};

struct pg_widget {
    const struct pg_widget_class *cls;
    struct pg_window *window;
    struct pg_widget *parent, *first, *last, *next, *prev;
    struct pg_rect rect;
    int req_w, req_h;
    int margin_l, margin_t, margin_r, margin_b;
    uint8_t halign, valign;
    bool hexpand, vexpand;
    bool visible, enabled, focusable, clip;
    bool hovered, pressed, focused;
    int col, row, col_span, row_span;
    struct pg_slot slots[PG_SIGNAL_COUNT];
    void *user;
};

struct pg_widget *pg_widget_new(const struct pg_widget_class *cls);
void pg_widget_destroy(struct pg_widget *w);
void pg_widget_add(struct pg_widget *parent, struct pg_widget *child);
void pg_widget_remove(struct pg_widget *child);
void pg_widget_clear(struct pg_widget *parent);
bool pg_widget_is(const struct pg_widget *w, const struct pg_widget_class *cls);
int pg_widget_child_count(const struct pg_widget *w);
struct pg_widget *pg_widget_child_at(const struct pg_widget *w, int index);

void pg_widget_set_visible(struct pg_widget *w, bool visible);
void pg_widget_set_enabled(struct pg_widget *w, bool enabled);
bool pg_widget_is_enabled(const struct pg_widget *w);
void pg_widget_set_size(struct pg_widget *w, int width, int height);
void pg_widget_set_margin(struct pg_widget *w, int margin);
void pg_widget_set_margins(struct pg_widget *w, int left, int top, int right, int bottom);
void pg_widget_set_align(struct pg_widget *w, enum pg_align h, enum pg_align v);
void pg_widget_set_expand(struct pg_widget *w, bool h, bool v);
void pg_widget_set_user(struct pg_widget *w, void *user);
void *pg_widget_user(const struct pg_widget *w);

void pg_widget_on(struct pg_widget *w, enum pg_signal signal, pg_callback fn, void *data);
void pg_widget_emit(struct pg_widget *w, enum pg_signal signal);

void pg_widget_queue_draw(struct pg_widget *w);
void pg_widget_queue_layout(struct pg_widget *w);
void pg_widget_grab_focus(struct pg_widget *w);
bool pg_widget_has_focus(const struct pg_widget *w);
bool pg_widget_focus_visible(const struct pg_widget *w);

void pg_widget_measure(struct pg_widget *w, int *width, int *height);
int pg_widget_height_for_width(struct pg_widget *w, int width);
void pg_widget_place(struct pg_widget *w, int x, int y, int width, int height);
void pg_widget_draw_focus(struct pg_widget *w, struct pg_canvas *c, int radius);
