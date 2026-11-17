#include "internal.h"

struct pg_widget *pg_widget_new(const struct pg_widget_class *cls)
{
    size_t size = cls && cls->size >= sizeof(struct pg_widget) ? cls->size : sizeof(struct pg_widget);
    struct pg_widget *w = calloc(1, size);
    if (!w)
        return NULL;
    w->cls = cls;
    w->visible = true;
    w->enabled = true;
    w->req_w = -1;
    w->req_h = -1;
    w->col_span = 1;
    w->row_span = 1;
    return w;
}

static void set_window_tree(struct pg_widget *w, struct pg_window *win)
{
    w->window = win;
    for (struct pg_widget *c = w->first; c; c = c->next)
        set_window_tree(c, win);
}

static void unlink_child(struct pg_widget *child)
{
    struct pg_widget *p = child->parent;
    if (!p)
        return;
    if (child->prev)
        child->prev->next = child->next;
    else
        p->first = child->next;
    if (child->next)
        child->next->prev = child->prev;
    else
        p->last = child->prev;
    child->parent = child->next = child->prev = NULL;
}

void pg_widget_add(struct pg_widget *parent, struct pg_widget *child)
{
    if (!parent || !child || child == parent)
        return;
    if (child->parent)
        pg_widget_remove(child);
    child->parent = parent;
    child->prev = parent->last;
    child->next = NULL;
    if (parent->last)
        parent->last->next = child;
    else
        parent->first = child;
    parent->last = child;
    set_window_tree(child, parent->window);
    pg_widget_queue_layout(parent);
}

void pg_widget_remove(struct pg_widget *child)
{
    if (!child || !child->parent)
        return;
    struct pg_widget *parent = child->parent;
    if (child->window)
        pg__window_forget(child->window, child);
    unlink_child(child);
    set_window_tree(child, NULL);
    pg_widget_queue_layout(parent);
}

static void destroy_tree(struct pg_widget *w)
{
    if (w->slots[PG_SIGNAL_DESTROY].fn)
        w->slots[PG_SIGNAL_DESTROY].fn(w, w->slots[PG_SIGNAL_DESTROY].data);
    while (w->first)
    {
        struct pg_widget *c = w->first;
        unlink_child(c);
        destroy_tree(c);
    }
    if (w->cls && w->cls->destroy)
        w->cls->destroy(w);
    free(w);
}

void pg_widget_destroy(struct pg_widget *w)
{
    if (!w)
        return;
    if (w->window)
        pg__window_forget(w->window, w);
    if (w->window && w->window->child == w)
        w->window->child = NULL;
    struct pg_widget *parent = w->parent;
    unlink_child(w);
    if (parent)
        pg_widget_queue_layout(parent);
    destroy_tree(w);
}

void pg_widget_clear(struct pg_widget *parent)
{
    while (parent && parent->first)
        pg_widget_destroy(parent->first);
}

bool pg_widget_is(const struct pg_widget *w, const struct pg_widget_class *cls)
{
    return w && w->cls == cls;
}

int pg_widget_child_count(const struct pg_widget *w)
{
    int n = 0;
    for (const struct pg_widget *c = w ? w->first : NULL; c; c = c->next)
        n++;
    return n;
}

struct pg_widget *pg_widget_child_at(const struct pg_widget *w, int index)
{
    struct pg_widget *c = w ? w->first : NULL;
    while (c && index-- > 0)
        c = c->next;
    return index < 0 ? c : NULL;
}

void pg_widget_set_visible(struct pg_widget *w, bool visible)
{
    if (!w || w->visible == visible)
        return;
    if (!visible && w->window)
        pg__window_forget(w->window, w);
    w->visible = visible;
    pg_widget_queue_layout(w);
}

void pg_widget_set_enabled(struct pg_widget *w, bool enabled)
{
    if (!w || w->enabled == enabled)
        return;
    w->enabled = enabled;
    if (!enabled && w->window)
        pg__window_forget(w->window, w);
    pg_widget_queue_draw(w);
}

bool pg_widget_is_enabled(const struct pg_widget *w)
{
    for (; w; w = w->parent)
        if (!w->enabled || !w->visible)
            return false;
    return true;
}

void pg_widget_set_size(struct pg_widget *w, int width, int height)
{
    if (!w)
        return;
    w->req_w = width;
    w->req_h = height;
    pg_widget_queue_layout(w);
}

void pg_widget_set_margin(struct pg_widget *w, int m)
{
    pg_widget_set_margins(w, m, m, m, m);
}

void pg_widget_set_margins(struct pg_widget *w, int l, int t, int r, int b)
{
    if (!w)
        return;
    w->margin_l = l;
    w->margin_t = t;
    w->margin_r = r;
    w->margin_b = b;
    pg_widget_queue_layout(w);
}

void pg_widget_set_align(struct pg_widget *w, enum pg_align h, enum pg_align v)
{
    if (!w)
        return;
    w->halign = (uint8_t)h;
    w->valign = (uint8_t)v;
    pg_widget_queue_layout(w);
}

void pg_widget_set_expand(struct pg_widget *w, bool h, bool v)
{
    if (!w)
        return;
    w->hexpand = h;
    w->vexpand = v;
    pg_widget_queue_layout(w);
}

void pg_widget_set_user(struct pg_widget *w, void *user)
{
    if (w)
        w->user = user;
}

void *pg_widget_user(const struct pg_widget *w)
{
    return w ? w->user : NULL;
}

void pg_widget_on(struct pg_widget *w, enum pg_signal signal, pg_callback fn, void *data)
{
    if (!w || signal < 0 || signal >= PG_SIGNAL_COUNT)
        return;
    w->slots[signal].fn = fn;
    w->slots[signal].data = data;
}

void pg_widget_emit(struct pg_widget *w, enum pg_signal signal)
{
    if (!w || signal < 0 || signal >= PG_SIGNAL_COUNT)
        return;
    if (w->slots[signal].fn)
        w->slots[signal].fn(w, w->slots[signal].data);
}

void pg_widget_queue_draw(struct pg_widget *w)
{
    if (!w || !w->window)
        return;
    pg_window_damage(w->window, w->rect.x - 4, w->rect.y - 4, w->rect.w + 8, w->rect.h + 8);
}

void pg_widget_queue_layout(struct pg_widget *w)
{
    if (!w || !w->window)
        return;
    w->window->layout_needed = true;
}

void pg_widget_grab_focus(struct pg_widget *w)
{
    if (w && w->window)
        pg_window_set_focus(w->window, w);
}

bool pg_widget_has_focus(const struct pg_widget *w)
{
    return w && w->focused && w->window && w->window->active;
}

bool pg_widget_focus_visible(const struct pg_widget *w)
{
    return pg_widget_has_focus(w) && w->window->focus_visible;
}

void pg_widget_measure(struct pg_widget *w, int *width, int *height)
{
    int mw = 0, mh = 0;
    if (w && w->visible)
    {
        if (w->cls && w->cls->measure)
            w->cls->measure(w, &mw, &mh);
        if (w->req_w >= 0)
            mw = w->req_w;
        if (w->req_h >= 0)
            mh = w->req_h;
        mw += w->margin_l + w->margin_r;
        mh += w->margin_t + w->margin_b;
    }
    if (width)
        *width = mw;
    if (height)
        *height = mh;
}

int pg_widget_height_for_width(struct pg_widget *w, int width)
{
    if (!w || !w->visible)
        return 0;
    int inner = width - w->margin_l - w->margin_r;
    int h;
    if (w->req_h >= 0)
    {
        h = w->req_h;
    }
    else if (w->cls && w->cls->height_for_width)
    {
        if (w->req_w >= 0 && w->halign != PG_ALIGN_FILL)
            inner = pg__min(inner, w->req_w);
        h = w->cls->height_for_width(w, pg__max(inner, 0));
    }
    else
    {
        int mw = 0;
        h = 0;
        if (w->cls && w->cls->measure)
            w->cls->measure(w, &mw, &h);
    }
    return h + w->margin_t + w->margin_b;
}

static int align_pos(int start, int avail, int size, uint8_t align)
{
    if (align == PG_ALIGN_CENTER)
        return start + (avail - size) / 2;
    if (align == PG_ALIGN_END)
        return start + avail - size;
    return start;
}

void pg_widget_place(struct pg_widget *w, int x, int y, int width, int height)
{
    if (!w)
        return;
    int ix = x + w->margin_l, iy = y + w->margin_t;
    int iw = pg__max(0, width - w->margin_l - w->margin_r);
    int ih = pg__max(0, height - w->margin_t - w->margin_b);
    int cw = iw, ch = ih;
    if (w->halign != PG_ALIGN_FILL || w->valign != PG_ALIGN_FILL)
    {
        int nw = 0, nh = 0;
        pg_widget_measure(w, &nw, &nh);
        nw -= w->margin_l + w->margin_r;
        if (w->halign != PG_ALIGN_FILL)
            cw = pg__min(iw, nw);
        if (w->valign != PG_ALIGN_FILL)
        {
            int hh = pg_widget_height_for_width(w, cw + w->margin_l + w->margin_r) - w->margin_t - w->margin_b;
            ch = pg__min(ih, hh);
        }
    }
    w->rect.x = align_pos(ix, iw, cw, w->halign);
    w->rect.y = align_pos(iy, ih, ch, w->valign);
    w->rect.w = cw;
    w->rect.h = ch;
    if (w->cls && w->cls->arrange)
        w->cls->arrange(w);
}

void pg_widget_draw_focus(struct pg_widget *w, struct pg_canvas *c, int radius)
{
    if (!pg_widget_focus_visible(w))
        return;
    pgc_rrect_stroke(c, w->rect.x - 2, w->rect.y - 2, w->rect.w + 4, w->rect.h + 4, radius + 2,
                     pg_theme()->accent, 170);
}

void pg__draw_widget(struct pg_widget *w, struct pg_canvas *c)
{
    if (!w->visible || w->rect.w <= 0 || w->rect.h <= 0)
        return;
    struct pg_rect clip = {c->cx0, c->cy0, c->cx1 - c->cx0, c->cy1 - c->cy0};
    struct pg_rect area = {w->rect.x - 12, w->rect.y - 12, w->rect.w + 24, w->rect.h + 24};
    if (!pg__rect_overlap(area, clip))
        return;
    if (w->cls && w->cls->draw)
        w->cls->draw(w, c);
    if (w->first)
    {
        struct pg_clip saved = {0};
        if (w->clip)
            saved = pgc_clip_push(c, w->rect.x, w->rect.y, w->rect.w, w->rect.h);
        for (struct pg_widget *ch = w->first; ch; ch = ch->next)
            pg__draw_widget(ch, c);
        if (w->clip)
            pgc_clip_pop(c, saved);
    }
    if (w->cls && w->cls->draw_after)
        w->cls->draw_after(w, c);
}

struct pg_widget *pg__hit(struct pg_widget *w, int x, int y)
{
    if (!w || !w->visible || !pg__rect_has(w->rect, x, y))
        return NULL;
    for (struct pg_widget *c = w->last; c; c = c->prev)
    {
        struct pg_widget *h = pg__hit(c, x, y);
        if (h)
            return h;
    }
    return w;
}
