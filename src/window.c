#include <stdio.h>
#include "internal.h"

#define DOUBLE_CLICK_MS 400
#define BUTTON_W 46

enum
{
    TB_NONE = 0,
    TB_MIN,
    TB_MAX,
    TB_CLOSE
};

static struct pg_window *windows;

struct pg_window *pg__windows(void)
{
    return windows;
}

struct pg_window *pg__window_of(struct vb_window *vb)
{
    for (struct pg_window *w = windows; w && vb; w = w->next)
        if (w->vb == vb)
            return w;
    return NULL;
}

bool pg__have_windows(void)
{
    for (struct pg_window *w = windows; w; w = w->next)
        if (!w->dead)
            return true;
    return false;
}

void pg__relayout_all(void)
{
    for (struct pg_window *w = windows; w; w = w->next)
    {
        w->layout_needed = true;
        pg_window_queue_draw(w);
    }
}

static bool decorated(const struct pg_window *win)
{
    return win->role == VB_ROLE_NORMAL && !(win->flags & PG_WINDOW_FRAMELESS) && win->state != VB_STATE_FULLSCREEN;
}

static int border(const struct pg_window *win)
{
    return decorated(win) && win->state == VB_STATE_NORMAL ? PG_BORDER : 0;
}

static int title_h(const struct pg_window *win)
{
    return decorated(win) ? PG_TITLEBAR_HEIGHT : 0;
}

struct pg_rect pg_window_content(const struct pg_window *win)
{
    struct pg_rect r = {0, 0, 0, 0};
    if (!win)
        return r;
    int b = border(win), t = title_h(win);
    r.x = b;
    r.y = b + t;
    r.w = pg__max(0, win->width - 2 * b);
    r.h = pg__max(0, win->height - 2 * b - t);
    return r;
}

static bool has_max(const struct pg_window *win)
{
    return (win->flags & PG_WINDOW_RESIZABLE) && !(win->flags & PG_WINDOW_NO_MAXIMIZE);
}

static bool has_min(const struct pg_window *win)
{
    return !(win->flags & PG_WINDOW_NO_MINIMIZE);
}

static struct pg_rect title_button(const struct pg_window *win, int which)
{
    int b = border(win);
    int slot = which == TB_CLOSE ? 1 : which == TB_MAX ? 2
                                                       : (has_max(win) ? 3 : 2);
    struct pg_rect r = {win->width - b - slot * BUTTON_W, b, BUTTON_W, title_h(win)};
    return r;
}

static int title_button_at(const struct pg_window *win, int x, int y)
{
    if (!decorated(win))
        return TB_NONE;
    static const int order[3] = {TB_CLOSE, TB_MAX, TB_MIN};
    for (int i = 0; i < 3; i++)
    {
        int which = order[i];
        if (which == TB_MAX && !has_max(win))
            continue;
        if (which == TB_MIN && !has_min(win))
            continue;
        if (pg__rect_has(title_button(win, which), x, y))
            return which;
    }
    return TB_NONE;
}

static bool in_titlebar(const struct pg_window *win, int x, int y)
{
    int b = border(win);
    return decorated(win) && y >= b && y < b + title_h(win) && x >= b && x < win->width - b;
}

static uint32_t edges_at(const struct pg_window *win, int x, int y)
{
    if (!decorated(win) || win->state != VB_STATE_NORMAL || !(win->flags & PG_WINDOW_RESIZABLE))
        return 0;
    uint32_t e = 0;
    int g = PG_RESIZE_GRIP, corner = PG_RESIZE_GRIP * 2;
    bool left = x < g, right = x >= win->width - g, top = y < g - 2, bottom = y >= win->height - g;
    if ((left || right) && (y < corner || y >= win->height - corner))
    {
        top = y < corner;
        bottom = y >= win->height - corner;
    }
    if ((top || bottom) && (x < corner || x >= win->width - corner))
    {
        left = x < corner;
        right = x >= win->width - corner;
    }
    if (left)
        e |= VB_EDGE_LEFT;
    if (right)
        e |= VB_EDGE_RIGHT;
    if (top)
        e |= VB_EDGE_TOP;
    if (bottom)
        e |= VB_EDGE_BOTTOM;
    return e;
}

static int cursor_for(uint32_t e)
{
    if ((e & VB_EDGE_LEFT && e & VB_EDGE_TOP) || (e & VB_EDGE_RIGHT && e & VB_EDGE_BOTTOM))
        return VB_CURSOR_RESIZE_NWSE;
    if ((e & VB_EDGE_RIGHT && e & VB_EDGE_TOP) || (e & VB_EDGE_LEFT && e & VB_EDGE_BOTTOM))
        return VB_CURSOR_RESIZE_NESW;
    if (e & (VB_EDGE_LEFT | VB_EDGE_RIGHT))
        return VB_CURSOR_RESIZE_H;
    if (e & (VB_EDGE_TOP | VB_EDGE_BOTTOM))
        return VB_CURSOR_RESIZE_V;
    return VB_CURSOR_ARROW;
}

static void set_cursor(struct pg_window *win, int cursor)
{
    if (win->cursor == cursor || !win->vb)
        return;
    win->cursor = cursor;
    vb_window_set_cursor(win->vb, cursor);
}

static struct pg_window *create(const char *title, int x, int y, int width, int height, uint32_t flags)
{
    struct pg_window *win = calloc(1, sizeof(*win));
    if (!win)
        return NULL;
    win->flags = flags;
    win->role = (flags & PG_WINDOW_POPUP)        ? VB_ROLE_POPUP
                : (flags & PG_WINDOW_PANEL)      ? VB_ROLE_PANEL
                : (flags & PG_WINDOW_BACKGROUND) ? VB_ROLE_BACKGROUND
                                                 : VB_ROLE_NORMAL;
    if (win->role != VB_ROLE_NORMAL)
        win->flags |= PG_WINDOW_FRAMELESS;
    win->active = win->role == VB_ROLE_POPUP;
    snprintf(win->title, sizeof(win->title), "%s", title ? title : "");
    int tw = width + 2 * border(win), th = height + 2 * border(win) + title_h(win);
    win->min_w = pg__min(tw, decorated(win) ? 160 : 1);
    win->min_h = pg__min(th, decorated(win) ? title_h(win) + 40 : 1);
    if (x < 0 && y < 0 && win->role == VB_ROLE_NORMAL)
        win->vb = vb_window_create(tw, th, win->title);
    else
        win->vb = vb_window_create_role(x, y, tw, th, win->title, win->role);
    if (!win->vb)
    {
        free(win);
        return NULL;
    }
    win->width = vb_window_width(win->vb);
    win->height = vb_window_height(win->vb);
    pgc_init(&win->canvas, vb_window_pixels(win->vb), win->width, win->height);
    win->layout_needed = true;
    win->cursor = VB_CURSOR_ARROW;
    pg_window_queue_draw(win);
    win->next = windows;
    windows = win;
    return win;
}

struct pg_window *pg_window_new(const char *title, int width, int height, uint32_t flags)
{
    return create(title, -1, -1, width, height, flags);
}

struct pg_window *pg_window_new_at(const char *title, int x, int y, int width, int height, uint32_t flags)
{
    return create(title, x, y, width, height, flags);
}

struct pg_window *pg__popup_new(struct pg_window *owner, int x, int y, int width, int height,
                                void (*done)(struct pg_window *win, void *data), void *data)
{
    int sw = 0, sh = 0;
    vb_screen_size(&sw, &sh);
    if (sw > 0 && x + width > sw)
        x = sw - width;
    if (sh > 0 && y + height > sh)
        y = sh - height;
    struct pg_window *win = create("", pg__max(0, x), pg__max(0, y), width, height, PG_WINDOW_POPUP);
    if (!win)
        return NULL;
    win->owner = owner;
    win->popup_done = done;
    win->popup_data = data;
    return win;
}

void pg__window_forget(struct pg_window *win, struct pg_widget *w)
{
    struct pg_widget **slots[4] = {&win->hover, &win->grab, &win->focus, &win->click_widget};
    for (int i = 0; i < 4; i++)
    {
        for (struct pg_widget *p = *slots[i]; p; p = p->parent)
        {
            if (p != w)
                continue;
            struct pg_widget *old = *slots[i];
            *slots[i] = NULL;
            if (i == 0)
                for (struct pg_widget *q = old; q; q = q->parent)
                    q->hovered = false;
            if (i == 1)
                old->pressed = false;
            if (i == 2)
                old->focused = false;
            break;
        }
    }
}

void pg_window_destroy(struct pg_window *win)
{
    if (!win || win->dead)
        return;
    win->dead = true;
    if (win->popup_done)
    {
        void (*done)(struct pg_window *, void *) = win->popup_done;
        win->popup_done = NULL;
        done(win, win->popup_data);
    }
    for (struct pg_window *w = windows; w; w = w->next)
        if (w->owner == win)
            pg_window_destroy(w);
    if (win->vb)
    {
        vb_window_destroy(win->vb);
        win->vb = NULL;
    }
}

void pg__reap(void)
{
    struct pg_window **pp = &windows;
    while (*pp)
    {
        struct pg_window *w = *pp;
        if (!w->dead)
        {
            pp = &w->next;
            continue;
        }
        *pp = w->next;
        if (w->child)
        {
            struct pg_widget *c = w->child;
            w->child = NULL;
            pg_widget_destroy(c);
        }
        free(w);
    }
}

void pg_window_close(struct pg_window *win)
{
    if (!win || win->dead)
        return;
    if (win->on_close && !win->on_close(win, win->close_data))
        return;
    pg_window_destroy(win);
}

static void set_tree_window(struct pg_widget *w, struct pg_window *win)
{
    w->window = win;
    for (struct pg_widget *c = w->first; c; c = c->next)
        set_tree_window(c, win);
}

void pg_window_set_child(struct pg_window *win, struct pg_widget *child)
{
    if (!win)
        return;
    if (win->child && win->child != child)
    {
        struct pg_widget *old = win->child;
        pg__window_forget(win, old);
        win->child = NULL;
        set_tree_window(old, NULL);
    }
    if (child && child->parent)
        pg_widget_remove(child);
    win->child = child;
    if (child)
        set_tree_window(child, win);
    win->layout_needed = true;
    pg_window_queue_draw(win);
}

struct pg_widget *pg_window_child(const struct pg_window *win)
{
    return win ? win->child : NULL;
}

void pg_window_set_title(struct pg_window *win, const char *title)
{
    if (!win)
        return;
    snprintf(win->title, sizeof(win->title), "%s", title ? title : "");
    if (win->vb)
        vb_window_title(win->vb, win->title);
    pg_window_damage(win, 0, 0, win->width, title_h(win) + border(win));
}

const char *pg_window_title(const struct pg_window *win)
{
    return win ? win->title : "";
}

void pg_window_set_background(struct pg_window *win, uint32_t color)
{
    if (!win)
        return;
    win->has_background = true;
    win->background = color;
    pg_window_queue_draw(win);
}

void pg_window_set_min_size(struct pg_window *win, int width, int height)
{
    if (!win)
        return;
    win->min_w = width + 2 * border(win);
    win->min_h = height + 2 * border(win) + title_h(win);
}

static void resize_buffer(struct pg_window *win, int width, int height)
{
    if (!win->vb || (width == win->width && height == win->height))
        return;
    if (vb_window_resize(win->vb, width, height) != 0)
        return;
    win->width = vb_window_width(win->vb);
    win->height = vb_window_height(win->vb);
    pgc_init(&win->canvas, vb_window_pixels(win->vb), win->width, win->height);
    win->layout_needed = true;
    pg_window_queue_draw(win);
}

bool pg_window_resize(struct pg_window *win, int width, int height)
{
    if (!win || !win->vb)
        return false;
    int tw = width + 2 * border(win), th = height + 2 * border(win) + title_h(win);
    resize_buffer(win, tw, th);
    return win->width == tw && win->height == th;
}

void pg_window_size(const struct pg_window *win, int *width, int *height)
{
    struct pg_rect r = pg_window_content(win);
    if (width)
        *width = r.w;
    if (height)
        *height = r.h;
}

void pg_window_screen_position(const struct pg_window *win, int *x, int *y)
{
    int px = 0, py = 0;
    if (win && win->vb)
        vb_window_position(win->vb, &px, &py);
    if (x)
        *x = px;
    if (y)
        *y = py;
}

static void request_state(struct pg_window *win, int state)
{
    if (win && win->vb)
        vb_window_set_state(win->vb, state);
}

void pg_window_maximize(struct pg_window *win)
{
    request_state(win, VB_STATE_MAXIMIZED);
}

void pg_window_restore(struct pg_window *win)
{
    request_state(win, VB_STATE_NORMAL);
}

void pg_window_minimize(struct pg_window *win)
{
    request_state(win, VB_STATE_MINIMIZED);
}

void pg_window_fullscreen(struct pg_window *win)
{
    request_state(win, VB_STATE_FULLSCREEN);
}

void pg_window_toggle_maximize(struct pg_window *win)
{
    if (!win)
        return;
    request_state(win, win->state == VB_STATE_NORMAL ? VB_STATE_MAXIMIZED : VB_STATE_NORMAL);
}

bool pg_window_is_maximized(const struct pg_window *win)
{
    return win && win->state == VB_STATE_MAXIMIZED;
}

bool pg_window_is_active(const struct pg_window *win)
{
    return win && win->active;
}

void pg_window_raise(struct pg_window *win)
{
    if (win && win->vb)
        vb_window_raise(win->vb);
}

void pg_window_on_close(struct pg_window *win, pg_window_close_fn fn, void *data)
{
    if (!win)
        return;
    win->on_close = fn;
    win->close_data = data;
}

void pg_window_on_key(struct pg_window *win, pg_window_key_fn fn, void *data)
{
    if (!win)
        return;
    win->on_key = fn;
    win->key_data = data;
}

void pg_window_set_user(struct pg_window *win, void *user)
{
    if (win)
        win->user = user;
}

void *pg_window_user(const struct pg_window *win)
{
    return win ? win->user : NULL;
}

static bool deliver(struct pg_widget *w, const struct pg_event *e)
{
    if (!w || !pg_widget_is_enabled(w) || !w->cls || !w->cls->event)
        return false;
    return w->cls->event(w, e);
}

static struct pg_widget *bubble(struct pg_widget *w, const struct pg_event *e)
{
    for (; w; w = w->parent)
        if (deliver(w, e))
            return w;
    return NULL;
}

void pg_window_set_focus(struct pg_window *win, struct pg_widget *w)
{
    if (!win || win->focus == w)
        return;
    struct pg_widget *old = win->focus;
    win->focus = w;
    struct pg_event e = {0};
    if (old)
    {
        old->focused = false;
        e.type = PG_EVENT_FOCUS_OUT;
        deliver(old, &e);
        pg_widget_queue_draw(old);
    }
    if (w)
    {
        w->focused = true;
        e.type = PG_EVENT_FOCUS_IN;
        deliver(w, &e);
        pg_widget_queue_draw(w);
    }
}

struct pg_widget *pg_window_focus(const struct pg_window *win)
{
    return win ? win->focus : NULL;
}

void pg_window_damage(struct pg_window *win, int x, int y, int width, int height)
{
    if (!win || win->dead)
        return;
    int x0 = pg__max(0, x), y0 = pg__max(0, y);
    int x1 = pg__min(win->width, x + width), y1 = pg__min(win->height, y + height);
    if (x1 <= x0 || y1 <= y0)
        return;
    if (!win->dirty_any)
    {
        win->dirty = (struct pg_rect){x0, y0, x1 - x0, y1 - y0};
        win->dirty_any = true;
        return;
    }
    int dx0 = pg__min(win->dirty.x, x0), dy0 = pg__min(win->dirty.y, y0);
    int dx1 = pg__max(win->dirty.x + win->dirty.w, x1), dy1 = pg__max(win->dirty.y + win->dirty.h, y1);
    win->dirty = (struct pg_rect){dx0, dy0, dx1 - dx0, dy1 - dy0};
}

void pg_window_queue_draw(struct pg_window *win)
{
    if (win)
        pg_window_damage(win, 0, 0, win->width, win->height);
}

uint32_t *pg_window_pixels(struct pg_window *win, int *stride)
{
    if (!win || !win->vb)
        return NULL;
    if (stride)
        *stride = win->canvas.stride;
    return win->canvas.px;
}

static void draw_glyph(struct pg_canvas *c, int which, struct pg_rect r, uint32_t color, bool maximized)
{
    float cx = (float)r.x + (float)r.w / 2.0f, cy = (float)r.y + (float)r.h / 2.0f;
    if (which == TB_MIN)
    {
        pgc_line(c, cx - 5.0f, cy + 0.5f, cx + 5.0f, cy + 0.5f, 1.2f, color);
    }
    else if (which == TB_MAX)
    {
        int x = (int)cx - 5, y = (int)cy - 5;
        if (maximized)
        {
            pgc_rrect_stroke(c, x + 2, y, 8, 8, 2, color, 200);
            pgc_fill(c, x, y + 2, 8, 8, pg_theme()->titlebar);
            pgc_rrect_stroke(c, x, y + 2, 8, 8, 2, color, 255);
        }
        else
        {
            pgc_rrect_stroke(c, x, y, 10, 10, 2, color, 255);
        }
    }
    else
    {
        pgc_line(c, cx - 5.0f, cy - 5.0f, cx + 5.0f, cy + 5.0f, 1.3f, color);
        pgc_line(c, cx + 5.0f, cy - 5.0f, cx - 5.0f, cy + 5.0f, 1.3f, color);
    }
}

static void draw_decor(struct pg_window *win)
{
    struct pg_canvas *c = &win->canvas;
    const struct pg_theme *t = pg_theme();
    int b = border(win), th = title_h(win);
    pgc_fill(c, b, b, win->width - 2 * b, th, t->titlebar);
    int right = win->width - b - BUTTON_W;
    static const int order[3] = {TB_CLOSE, TB_MAX, TB_MIN};
    for (int i = 0; i < 3; i++)
    {
        int which = order[i];
        if ((which == TB_MAX && !has_max(win)) || (which == TB_MIN && !has_min(win)))
            continue;
        struct pg_rect r = title_button(win, which);
        right = r.x;
        bool hot = win->title_hot == which;
        bool down = hot && win->title_pressed == which;
        uint32_t fg = win->active ? t->text2 : t->text3;
        if (hot && which == TB_CLOSE)
        {
            pgc_fill(c, r.x, r.y, r.w, r.h, down ? pgc_lighten(t->danger, -30) : t->danger);
            fg = 0xffffff;
        }
        else if (hot)
        {
            pgc_fill(c, r.x, r.y, r.w, r.h, down ? t->surface3 : t->surface2);
            fg = t->text;
        }
        draw_glyph(c, which, r, fg, win->state == VB_STATE_MAXIMIZED);
    }
    int ty = b + (th - pgc_font_h(PGF_MEDIUM)) / 2;
    pgc_text_fit(c, PGF_MEDIUM, b + 14, ty, right - b - 24, win->title, win->active ? t->text : t->text3);
    pgc_fill_a(c, b, b + th - 1, win->width - 2 * b, 1, t->border, 110);
    if (b)
    {
        uint32_t col = win->active ? pgc_mix(t->border, t->accent, 70) : t->border;
        pgc_fill(c, 0, 0, win->width, 1, col);
        pgc_fill(c, 0, win->height - 1, win->width, 1, col);
        pgc_fill(c, 0, 0, 1, win->height, col);
        pgc_fill(c, win->width - 1, 0, 1, win->height, col);
    }
}

static void layout(struct pg_window *win)
{
    win->layout_needed = false;
    struct pg_rect r = pg_window_content(win);
    if (win->child)
        pg_widget_place(win->child, r.x, r.y, r.w, r.h);
    pg_window_queue_draw(win);
}

void pg_window_flush(struct pg_window *win)
{
    if (!win || win->dead || !win->vb)
        return;
    if (win->layout_needed)
        layout(win);
    if (!win->dirty_any)
        return;
    struct pg_canvas *c = &win->canvas;
    struct pg_rect d = win->dirty;
    win->dirty_any = false;
    pgc_clip(c, d.x, d.y, d.w, d.h);
    const struct pg_theme *t = pg_theme();
    struct pg_rect content = pg_window_content(win);
    uint32_t bg = win->has_background ? win->background : t->bg;
    pgc_fill(c, content.x, content.y, content.w, content.h, bg);
    if (decorated(win))
        draw_decor(win);
    else if (win->role == VB_ROLE_POPUP)
        pgc_fill(c, 0, 0, win->width, win->height, bg);
    if (win->child)
    {
        struct pg_clip saved = pgc_clip_push(c, content.x, content.y, content.w, content.h);
        pg__draw_widget(win->child, c);
        pgc_clip_pop(c, saved);
    }
    if (win->role == VB_ROLE_POPUP)
    {
        pgc_unclip(c);
        pgc_clip(c, d.x, d.y, d.w, d.h);
        pgc_fill(c, 0, 0, win->width, 1, t->border);
        pgc_fill(c, 0, win->height - 1, win->width, 1, t->border);
        pgc_fill(c, 0, 0, 1, win->height, t->border);
        pgc_fill(c, win->width - 1, 0, 1, win->height, t->border);
    }
    pgc_unclip(c);
    vb_window_damage(win->vb, d.x, d.y, d.w, d.h);
}

void pg__flush_all(void)
{
    for (struct pg_window *w = windows; w; w = w->next)
        pg_window_flush(w);
}

static bool chain_has(struct pg_widget *chain, struct pg_widget *w)
{
    for (; chain; chain = chain->parent)
        if (chain == w)
            return true;
    return false;
}

static void set_hover(struct pg_window *win, struct pg_widget *target)
{
    struct pg_widget *old = win->hover;
    if (old == target)
        return;
    struct pg_event e = {0};
    e.x = win->pointer_x;
    e.y = win->pointer_y;
    for (struct pg_widget *w = old; w; w = w->parent)
    {
        if (chain_has(target, w) || !w->hovered)
            continue;
        w->hovered = false;
        e.type = PG_EVENT_LEAVE;
        deliver(w, &e);
        pg_widget_queue_draw(w);
    }
    for (struct pg_widget *w = target; w; w = w->parent)
    {
        if (w->hovered)
            continue;
        w->hovered = true;
        e.type = PG_EVENT_ENTER;
        deliver(w, &e);
        pg_widget_queue_draw(w);
    }
    win->hover = target;
}

static struct pg_widget *target_at(struct pg_window *win, int x, int y)
{
    struct pg_rect content = pg_window_content(win);
    if (!win->child || !pg__rect_has(content, x, y))
        return NULL;
    return pg__hit(win->child, x, y);
}

static void damage_titlebar(struct pg_window *win)
{
    pg_window_damage(win, 0, 0, win->width, title_h(win) + border(win));
}

static void motion(struct pg_window *win, int x, int y, uint32_t buttons)
{
    struct pg_event e = {0};
    e.type = PG_EVENT_POINTER_MOVE;
    e.x = x;
    e.y = y;
    e.buttons = buttons;
    if (win->grab)
    {
        deliver(win->grab, &e);
        return;
    }
    int hot = title_button_at(win, x, y);
    if (hot != win->title_hot)
    {
        win->title_hot = hot;
        damage_titlebar(win);
    }
    set_cursor(win, cursor_for(edges_at(win, x, y)));
    struct pg_widget *target = target_at(win, x, y);
    set_hover(win, target);
    bubble(target, &e);
}

static void focus_chain(struct pg_window *win, struct pg_widget *target)
{
    struct pg_widget *f = target;
    while (f && !(f->focusable && pg_widget_is_enabled(f)))
        f = f->parent;
    pg_window_set_focus(win, f);
}

static void press(struct pg_window *win, int x, int y, uint32_t button, uint32_t buttons)
{
    win->focus_visible = false;
    uint64_t now = pg_time_ms();
    if (button == VB_BUTTON_LEFT)
    {
        int tb = title_button_at(win, x, y);
        if (tb)
        {
            win->title_pressed = tb;
            damage_titlebar(win);
            return;
        }
        uint32_t edges = edges_at(win, x, y);
        if (edges)
        {
            vb_window_start_resize(win->vb, edges);
            return;
        }
        if (in_titlebar(win, x, y))
        {
            if (has_max(win) && now - win->title_click_ms < DOUBLE_CLICK_MS)
            {
                win->title_click_ms = 0;
                pg_window_toggle_maximize(win);
            }
            else
            {
                win->title_click_ms = now;
                vb_window_move(win->vb);
            }
            return;
        }
    }
    struct pg_widget *target = target_at(win, x, y);
    if (button == VB_BUTTON_LEFT)
        focus_chain(win, target);
    if (target && target == win->click_widget && now - win->click_ms < DOUBLE_CLICK_MS)
        win->click_count++;
    else
        win->click_count = 1;
    win->click_widget = target;
    win->click_ms = now;
    struct pg_event e = {0};
    e.type = PG_EVENT_POINTER_DOWN;
    e.x = x;
    e.y = y;
    e.button = button;
    e.buttons = buttons;
    e.clicks = win->click_count;
    struct pg_widget *h = bubble(target, &e);
    if (h && !win->dead)
    {
        win->grab = h;
        h->pressed = true;
        pg_widget_queue_draw(h);
    }
}

static void release(struct pg_window *win, int x, int y, uint32_t button, uint32_t buttons)
{
    if (win->title_pressed)
    {
        int tb = win->title_pressed;
        win->title_pressed = 0;
        damage_titlebar(win);
        if (button != VB_BUTTON_LEFT || title_button_at(win, x, y) != tb)
            return;
        if (tb == TB_CLOSE)
            pg_window_close(win);
        else if (tb == TB_MAX)
            pg_window_toggle_maximize(win);
        else if (tb == TB_MIN)
            pg_window_minimize(win);
        return;
    }
    struct pg_widget *g = win->grab;
    if (!g)
        return;
    struct pg_event e = {0};
    e.type = PG_EVENT_POINTER_UP;
    e.x = x;
    e.y = y;
    e.button = button;
    e.buttons = buttons;
    e.clicks = win->click_count;
    deliver(g, &e);
    if (win->dead || buttons)
        return;
    if (win->grab == g)
    {
        win->grab = NULL;
        g->pressed = false;
        pg_widget_queue_draw(g);
    }
    motion(win, x, y, buttons);
}

static void wheel(struct pg_window *win, int x, int y, int amount)
{
    struct pg_event e = {0};
    e.type = PG_EVENT_WHEEL;
    e.x = x;
    e.y = y;
    e.wheel = amount;
    bubble(target_at(win, x, y), &e);
}

static void collect_focusable(struct pg_widget *w, struct pg_widget **out, int *n, int cap)
{
    if (!w || !w->visible || !w->enabled)
        return;
    if (w->focusable && *n < cap)
        out[(*n)++] = w;
    for (struct pg_widget *c = w->first; c; c = c->next)
        collect_focusable(c, out, n, cap);
}

extern void pg__scroll_reveal_ancestors(struct pg_widget *w);

static void focus_step(struct pg_window *win, int dir)
{
    struct pg_widget *list[512];
    int n = 0;
    collect_focusable(win->child, list, &n, 512);
    if (!n)
        return;
    int cur = -1;
    for (int i = 0; i < n; i++)
        if (list[i] == win->focus)
            cur = i;
    int next = cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
    win->focus_visible = true;
    pg_window_set_focus(win, list[next]);
    pg__scroll_reveal_ancestors(list[next]);
    pg_widget_queue_draw(list[next]);
}

static uint32_t text_char(const struct vb_event *v)
{
    if (v->mods & (VB_MOD_CTRL | VB_MOD_ALT | VB_MOD_GUI))
        return 0;
    char ch = vb_key_char(v);
    if ((unsigned char)ch < 32 || ch == 127)
        return 0;
    return (uint32_t)(unsigned char)ch;
}

static void key(struct pg_window *win, const struct vb_event *v)
{
    struct pg_event e = {0};
    e.type = v->pressed ? PG_EVENT_KEY_DOWN : PG_EVENT_KEY_UP;
    e.key = v->key;
    e.mods = v->mods;
    e.repeat = v->repeat;
    e.ch = v->pressed ? text_char(v) : 0;
    e.x = win->pointer_x;
    e.y = win->pointer_y;
    if (v->pressed && win->on_key && win->on_key(win, &e, win->key_data))
        return;
    if (win->dead)
        return;
    struct pg_widget *start = win->focus ? win->focus : win->child;
    if (bubble(start, &e) || win->dead || !v->pressed)
        return;
    if (e.key == PG_KEY_TAB)
    {
        focus_step(win, (e.mods & VB_MOD_SHIFT) ? -1 : 1);
        return;
    }
    if (e.key == PG_KEY_ESCAPE && win->role == VB_ROLE_POPUP)
    {
        pg_window_destroy(win);
    }
}

static void pointer(struct pg_window *win, const struct vb_event *v)
{
    int x = v->x, y = v->y;
    win->pointer_x = x;
    win->pointer_y = y;
    win->pointer_inside = true;
    uint32_t down = v->changed & v->buttons, up = v->changed & ~v->buttons;
    win->buttons = v->buttons;
    if (v->wheel)
        wheel(win, x, y, v->wheel);
    if (!v->changed && !v->wheel)
        motion(win, x, y, v->buttons);
    for (uint32_t bit = 1; bit <= 4 && !win->dead; bit <<= 1)
    {
        if (down & bit)
            press(win, x, y, bit, v->buttons);
        if (up & bit)
            release(win, x, y, bit, v->buttons);
    }
}

void pg__window_event(struct pg_window *win, const struct vb_event *v)
{
    switch (v->type)
    {
    case VB_EVENT_POINTER:
        pointer(win, v);
        break;
    case VB_EVENT_ENTER:
        win->pointer_inside = true;
        win->pointer_x = v->x;
        win->pointer_y = v->y;
        motion(win, v->x, v->y, win->buttons);
        break;
    case VB_EVENT_LEAVE:
        win->pointer_inside = false;
        if (!win->grab)
            set_hover(win, NULL);
        if (win->title_hot)
        {
            win->title_hot = 0;
            damage_titlebar(win);
        }
        win->cursor = -1;
        break;
    case VB_EVENT_KEY:
        key(win, v);
        break;
    case VB_EVENT_FOCUS:
        win->active = v->focused;
        if (!v->focused && win->grab)
        {
            win->grab->pressed = false;
            win->grab = NULL;
        }
        pg_window_queue_draw(win);
        break;
    case VB_EVENT_CLOSE:
        pg_window_close(win);
        break;
    case VB_EVENT_POPUP_DONE:
        pg_window_destroy(win);
        break;
    case VB_EVENT_CONFIGURE:
    {
        int st = (v->width > 0 || v->state) ? v->state : win->state;
        if (st != win->state)
        {
            win->state = st;
            win->layout_needed = true;
            pg_window_queue_draw(win);
        }
        if (v->width > 0 && v->height > 0)
            resize_buffer(win, pg__max(v->width, win->state == VB_STATE_NORMAL ? win->min_w : 1),
                          pg__max(v->height, win->state == VB_STATE_NORMAL ? win->min_h : 1));
        break;
    }
    default:
        break;
    }
}
