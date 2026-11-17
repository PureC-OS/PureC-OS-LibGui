#include "internal.h"

#define GRID_MAX 64

static bool wants_hexpand(struct pg_widget *w)
{
    if (!w->visible)
        return false;
    if (w->hexpand)
        return true;
    for (struct pg_widget *c = w->first; c; c = c->next)
        if (wants_hexpand(c))
            return true;
    return false;
}

static bool wants_vexpand(struct pg_widget *w)
{
    if (!w->visible)
        return false;
    if (w->vexpand)
        return true;
    for (struct pg_widget *c = w->first; c; c = c->next)
        if (wants_vexpand(c))
            return true;
    return false;
}

struct box {
    struct pg_widget base;
    int orientation;
    int spacing;
    int pl, pt, pr, pb;
    bool has_bg;
    uint32_t bg;
    int radius;
    int header;
};

static int visible_children(struct pg_widget *w)
{
    int n = 0;
    for (struct pg_widget *c = w->first; c; c = c->next)
        if (c->visible)
            n++;
    return n;
}

static void box_measure(struct pg_widget *w, int *width, int *height)
{
    struct box *b = (struct box *)w;
    int main = 0, cross = 0, n = 0;
    for (struct pg_widget *c = w->first; c; c = c->next) {
        if (!c->visible)
            continue;
        int cw, chh;
        pg_widget_measure(c, &cw, &chh);
        if (b->orientation == PG_VERTICAL) {
            main += chh;
            cross = pg__max(cross, cw);
        } else {
            main += cw;
            cross = pg__max(cross, chh);
        }
        n++;
    }
    if (n > 1)
        main += b->spacing * (n - 1);
    int px = b->pl + b->pr, py = b->pt + b->pb + b->header;
    if (b->orientation == PG_VERTICAL) {
        *width = cross + px;
        *height = main + py;
    } else {
        *width = main + px;
        *height = cross + py;
    }
}

static void horizontal_sizes(struct box *b, int inner, int *sizes, int count)
{
    struct pg_widget *w = &b->base;
    int total = 0, expanders = 0, i = 0;
    for (struct pg_widget *c = w->first; c && i < count; c = c->next) {
        if (!c->visible)
            continue;
        pg_widget_measure(c, &sizes[i], NULL);
        total += sizes[i];
        if (wants_hexpand(c))
            expanders++;
        i++;
    }
    total += b->spacing * pg__max(0, count - 1);
    int extra = inner - total;
    if (!expanders || !extra)
        return;
    i = 0;
    int left = extra, k = 0;
    for (struct pg_widget *c = w->first; c && i < count; c = c->next) {
        if (!c->visible)
            continue;
        if (wants_hexpand(c)) {
            k++;
            int add = k == expanders ? left : extra / expanders;
            left -= add;
            sizes[i] = pg__max(0, sizes[i] + add);
        }
        i++;
    }
}

static int box_hfw(struct pg_widget *w, int width)
{
    struct box *b = (struct box *)w;
    int inner = pg__max(0, width - b->pl - b->pr);
    int n = visible_children(w);
    int h = 0;
    if (b->orientation == PG_VERTICAL) {
        for (struct pg_widget *c = w->first; c; c = c->next)
            if (c->visible)
                h += pg_widget_height_for_width(c, inner);
        if (n > 1)
            h += b->spacing * (n - 1);
    } else if (n) {
        int *sizes = calloc((size_t)n, sizeof(int));
        if (!sizes)
            return 0;
        horizontal_sizes(b, inner, sizes, n);
        int i = 0;
        for (struct pg_widget *c = w->first; c; c = c->next)
            if (c->visible)
                h = pg__max(h, pg_widget_height_for_width(c, sizes[i++]));
        free(sizes);
    }
    return h + b->pt + b->pb + b->header;
}

static void box_arrange(struct pg_widget *w)
{
    struct box *b = (struct box *)w;
    int x = w->rect.x + b->pl, y = w->rect.y + b->pt + b->header;
    int iw = pg__max(0, w->rect.w - b->pl - b->pr), ih = pg__max(0, w->rect.h - b->pt - b->pb - b->header);
    int n = visible_children(w);
    if (!n)
        return;
    int *sizes = calloc((size_t)n, sizeof(int));
    if (!sizes)
        return;
    if (b->orientation == PG_HORIZONTAL) {
        horizontal_sizes(b, iw, sizes, n);
    } else {
        int total = 0, expanders = 0, i = 0;
        for (struct pg_widget *c = w->first; c; c = c->next) {
            if (!c->visible)
                continue;
            sizes[i] = pg_widget_height_for_width(c, iw);
            total += sizes[i++];
            if (wants_vexpand(c))
                expanders++;
        }
        total += b->spacing * (n - 1);
        int extra = ih - total;
        if (expanders && extra) {
            int left = extra, k = 0;
            i = 0;
            for (struct pg_widget *c = w->first; c; c = c->next) {
                if (!c->visible)
                    continue;
                if (wants_vexpand(c)) {
                    k++;
                    int add = k == expanders ? left : extra / expanders;
                    left -= add;
                    sizes[i] = pg__max(0, sizes[i] + add);
                }
                i++;
            }
        }
    }
    int i = 0, pos = b->orientation == PG_HORIZONTAL ? x : y;
    for (struct pg_widget *c = w->first; c; c = c->next) {
        if (!c->visible)
            continue;
        if (b->orientation == PG_HORIZONTAL)
            pg_widget_place(c, pos, y, sizes[i], ih);
        else
            pg_widget_place(c, x, pos, iw, sizes[i]);
        pos += sizes[i++] + b->spacing;
    }
    free(sizes);
}

static void box_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct box *b = (struct box *)w;
    if (b->has_bg)
        pgc_rrect(c, w->rect.x, w->rect.y, w->rect.w, w->rect.h, b->radius, pg_color(b->bg), 255);
}

static const struct pg_widget_class box_class = {
    .name = "box",
    .size = sizeof(struct box),
    .measure = box_measure,
    .height_for_width = box_hfw,
    .arrange = box_arrange,
    .draw = box_draw,
};

struct pg_widget *pg_box_new(enum pg_orientation orientation, int spacing)
{
    struct box *b = (struct box *)pg_widget_new(&box_class);
    if (!b)
        return NULL;
    b->orientation = orientation;
    b->spacing = spacing;
    return &b->base;
}

struct pg_widget *pg_hbox(int spacing)
{
    return pg_box_new(PG_HORIZONTAL, spacing);
}

struct pg_widget *pg_vbox(int spacing)
{
    return pg_box_new(PG_VERTICAL, spacing);
}

static struct box *as_box(struct pg_widget *w)
{
    return w && w->cls && w->cls->arrange == box_arrange ? (struct box *)w : NULL;
}

void pg_box_set_spacing(struct pg_widget *w, int spacing)
{
    struct box *b = as_box(w);
    if (!b)
        return;
    b->spacing = spacing;
    pg_widget_queue_layout(w);
}

void pg_box_set_padding(struct pg_widget *w, int l, int t, int r, int bo)
{
    struct box *b = as_box(w);
    if (!b)
        return;
    b->pl = l;
    b->pt = t;
    b->pr = r;
    b->pb = bo;
    pg_widget_queue_layout(w);
}

void pg_box_set_background(struct pg_widget *w, uint32_t color, int radius)
{
    struct box *b = as_box(w);
    if (!b)
        return;
    b->has_bg = true;
    b->bg = color;
    b->radius = radius;
    pg_widget_queue_draw(w);
}

struct card {
    struct box box;
    char *title;
    char *subtitle;
    int icon;
    int padding;
};

#define CARD_HEADER 36
#define CARD_GAP 14

static void card_sync(struct card *k)
{
    struct box *b = &k->box;
    b->pl = b->pr = b->pt = b->pb = k->padding;
    b->header = (k->title && k->title[0]) ? CARD_HEADER + CARD_GAP : 0;
}

static void card_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct card *k = (struct card *)w;
    const struct pg_theme *t = pg_theme();
    struct pg_rect r = w->rect;
    if (t->light)
        pgc_shadow(c, r.x, r.y, r.w, r.h, 12, 6, 50);
    pgc_rrect(c, r.x, r.y, r.w, r.h, 12, t->surface, 255);
    pgc_rrect_stroke(c, r.x, r.y, r.w, r.h, 12, t->border, t->light ? 160 : 120);
    if (!k->box.header)
        return;
    int x = r.x + k->padding, y = r.y + k->padding;
    int tx = x;
    if (k->icon >= 0) {
        int px = pgc_icon_px(PG_ICON_S);
        pgc_rrect(c, x, y, 34, 34, 11, t->accent, t->light ? 40 : 46);
        pgc_icon(c, k->icon, PG_ICON_S, x + (34 - px) / 2, y + (34 - px) / 2, t->accent);
        tx += 46;
    }
    int maxw = r.x + r.w - k->padding - tx;
    if (k->subtitle && k->subtitle[0]) {
        pgc_text_fit(c, PGF_HEAD, tx, y - 1, maxw, k->title, t->text);
        pgc_text_fit(c, PGF_SMALL, tx, y + 19, maxw, k->subtitle, t->text3);
    } else {
        pgc_text_fit(c, PGF_HEAD, tx, y + 7, maxw, k->title, t->text);
    }
}

static void card_destroy(struct pg_widget *w)
{
    struct card *k = (struct card *)w;
    free(k->title);
    free(k->subtitle);
}

static void card_measure(struct pg_widget *w, int *width, int *height)
{
    struct card *k = (struct card *)w;
    box_measure(w, width, height);
    if (k->box.header) {
        int tw = pg__max(pgc_text_w(PGF_HEAD, k->title), k->subtitle ? pgc_text_w(PGF_SMALL, k->subtitle) : 0);
        *width = pg__max(*width, tw + (k->icon >= 0 ? 46 : 0) + 2 * k->padding);
    }
}

static const struct pg_widget_class card_class = {
    .name = "card",
    .size = sizeof(struct card),
    .measure = card_measure,
    .height_for_width = box_hfw,
    .arrange = box_arrange,
    .draw = card_draw,
    .destroy = card_destroy,
};

struct pg_widget *pg_card_new(const char *title, const char *subtitle, int icon)
{
    struct card *k = (struct card *)pg_widget_new(&card_class);
    if (!k)
        return NULL;
    k->box.orientation = PG_VERTICAL;
    k->box.spacing = 12;
    k->padding = 18;
    k->icon = icon;
    k->title = title ? pg__strdup(title) : NULL;
    k->subtitle = subtitle ? pg__strdup(subtitle) : NULL;
    card_sync(k);
    return &k->box.base;
}

static struct card *as_card(struct pg_widget *w)
{
    return w && w->cls == &card_class ? (struct card *)w : NULL;
}

void pg_card_set_title(struct pg_widget *w, const char *title, const char *subtitle)
{
    struct card *k = as_card(w);
    if (!k)
        return;
    free(k->title);
    free(k->subtitle);
    k->title = title ? pg__strdup(title) : NULL;
    k->subtitle = subtitle ? pg__strdup(subtitle) : NULL;
    card_sync(k);
    pg_widget_queue_layout(w);
}

void pg_card_set_padding(struct pg_widget *w, int padding)
{
    struct card *k = as_card(w);
    if (!k)
        return;
    k->padding = padding;
    card_sync(k);
    pg_widget_queue_layout(w);
}

void pg_card_set_spacing(struct pg_widget *w, int spacing)
{
    struct card *k = as_card(w);
    if (!k)
        return;
    k->box.spacing = spacing;
    pg_widget_queue_layout(w);
}

struct grid {
    struct pg_widget base;
    int col_spacing, row_spacing;
};

struct grid_solution {
    int cols, rows;
    int colw[GRID_MAX];
    int rowh[GRID_MAX];
};

static int span_size(const int *sizes, int start, int span, int gap)
{
    int s = 0;
    for (int i = start; i < start + span && i < GRID_MAX; i++)
        s += sizes[i];
    return s + gap * (span - 1);
}

static void grid_solve(struct grid *g, int width, struct grid_solution *s)
{
    struct pg_widget *w = &g->base;
    memset(s, 0, sizeof(*s));
    bool col_expand[GRID_MAX] = {0};
    for (struct pg_widget *c = w->first; c; c = c->next) {
        if (!c->visible)
            continue;
        int col = pg__clamp(c->col, 0, GRID_MAX - 1), cs = pg__clamp(c->col_span, 1, GRID_MAX - col);
        int row = pg__clamp(c->row, 0, GRID_MAX - 1), rs = pg__clamp(c->row_span, 1, GRID_MAX - row);
        s->cols = pg__max(s->cols, col + cs);
        s->rows = pg__max(s->rows, row + rs);
        if (cs == 1) {
            int cw;
            pg_widget_measure(c, &cw, NULL);
            s->colw[col] = pg__max(s->colw[col], cw);
        }
        if (wants_hexpand(c))
            for (int i = col; i < col + cs; i++)
                col_expand[i] = true;
    }
    for (struct pg_widget *c = w->first; c; c = c->next) {
        if (!c->visible || c->col_span <= 1)
            continue;
        int col = pg__clamp(c->col, 0, GRID_MAX - 1), cs = pg__clamp(c->col_span, 1, GRID_MAX - col);
        int cw;
        pg_widget_measure(c, &cw, NULL);
        int have = span_size(s->colw, col, cs, g->col_spacing);
        if (cw > have)
            s->colw[col + cs - 1] += cw - have;
    }
    if (width >= 0 && s->cols) {
        int total = span_size(s->colw, 0, s->cols, g->col_spacing);
        int extra = width - total, n = 0;
        for (int i = 0; i < s->cols; i++)
            if (col_expand[i])
                n++;
        if (extra && n) {
            int left = extra, k = 0;
            for (int i = 0; i < s->cols; i++) {
                if (!col_expand[i])
                    continue;
                k++;
                int add = k == n ? left : extra / n;
                left -= add;
                s->colw[i] = pg__max(0, s->colw[i] + add);
            }
        }
    }
    for (struct pg_widget *c = w->first; c; c = c->next) {
        if (!c->visible)
            continue;
        int col = pg__clamp(c->col, 0, GRID_MAX - 1), cs = pg__clamp(c->col_span, 1, GRID_MAX - col);
        int row = pg__clamp(c->row, 0, GRID_MAX - 1), rs = pg__clamp(c->row_span, 1, GRID_MAX - row);
        int h = pg_widget_height_for_width(c, span_size(s->colw, col, cs, g->col_spacing));
        if (rs == 1)
            s->rowh[row] = pg__max(s->rowh[row], h);
        else {
            int have = span_size(s->rowh, row, rs, g->row_spacing);
            if (h > have)
                s->rowh[row + rs - 1] += h - have;
        }
    }
}

static void grid_measure(struct pg_widget *w, int *width, int *height)
{
    struct grid *g = (struct grid *)w;
    struct grid_solution s;
    grid_solve(g, -1, &s);
    *width = s.cols ? span_size(s.colw, 0, s.cols, g->col_spacing) : 0;
    *height = s.rows ? span_size(s.rowh, 0, s.rows, g->row_spacing) : 0;
}

static int grid_hfw(struct pg_widget *w, int width)
{
    struct grid *g = (struct grid *)w;
    struct grid_solution s;
    grid_solve(g, width, &s);
    return s.rows ? span_size(s.rowh, 0, s.rows, g->row_spacing) : 0;
}

static void grid_arrange(struct pg_widget *w)
{
    struct grid *g = (struct grid *)w;
    struct grid_solution s;
    grid_solve(g, w->rect.w, &s);
    bool row_expand[GRID_MAX] = {0};
    int n = 0;
    for (struct pg_widget *c = w->first; c; c = c->next)
        if (c->visible && wants_vexpand(c))
            for (int i = pg__clamp(c->row, 0, GRID_MAX - 1); i < c->row + c->row_span && i < GRID_MAX; i++)
                row_expand[i] = true;
    for (int i = 0; i < s.rows; i++)
        if (row_expand[i])
            n++;
    int extra = w->rect.h - (s.rows ? span_size(s.rowh, 0, s.rows, g->row_spacing) : 0);
    if (n && extra > 0) {
        int left = extra, k = 0;
        for (int i = 0; i < s.rows; i++) {
            if (!row_expand[i])
                continue;
            k++;
            int add = k == n ? left : extra / n;
            left -= add;
            s.rowh[i] += add;
        }
    }
    int xs[GRID_MAX + 1], ys[GRID_MAX + 1];
    xs[0] = w->rect.x;
    ys[0] = w->rect.y;
    for (int i = 0; i < GRID_MAX; i++) {
        xs[i + 1] = xs[i] + s.colw[i] + g->col_spacing;
        ys[i + 1] = ys[i] + s.rowh[i] + g->row_spacing;
    }
    for (struct pg_widget *c = w->first; c; c = c->next) {
        if (!c->visible)
            continue;
        int col = pg__clamp(c->col, 0, GRID_MAX - 1), cs = pg__clamp(c->col_span, 1, GRID_MAX - col);
        int row = pg__clamp(c->row, 0, GRID_MAX - 1), rs = pg__clamp(c->row_span, 1, GRID_MAX - row);
        pg_widget_place(c, xs[col], ys[row], span_size(s.colw, col, cs, g->col_spacing),
                        span_size(s.rowh, row, rs, g->row_spacing));
    }
}

static const struct pg_widget_class grid_class = {
    .name = "grid",
    .size = sizeof(struct grid),
    .measure = grid_measure,
    .height_for_width = grid_hfw,
    .arrange = grid_arrange,
};

struct pg_widget *pg_grid_new(int column_spacing, int row_spacing)
{
    struct grid *g = (struct grid *)pg_widget_new(&grid_class);
    if (!g)
        return NULL;
    g->col_spacing = column_spacing;
    g->row_spacing = row_spacing;
    return &g->base;
}

void pg_grid_attach(struct pg_widget *grid, struct pg_widget *child, int col, int row, int col_span, int row_span)
{
    if (!grid || !child || grid->cls != &grid_class)
        return;
    child->col = pg__clamp(col, 0, GRID_MAX - 1);
    child->row = pg__clamp(row, 0, GRID_MAX - 1);
    child->col_span = pg__clamp(col_span, 1, GRID_MAX - child->col);
    child->row_span = pg__clamp(row_span, 1, GRID_MAX - child->row);
    pg_widget_add(grid, child);
}

struct stack {
    struct pg_widget base;
    int index;
};

static void stack_sync(struct pg_widget *w)
{
    struct stack *s = (struct stack *)w;
    int i = 0;
    for (struct pg_widget *c = w->first; c; c = c->next, i++) {
        bool on = i == s->index;
        if (c->visible && !on && w->window)
            pg__window_forget(w->window, c);
        c->visible = on;
    }
}

static struct pg_widget *stack_current(struct pg_widget *w)
{
    stack_sync(w);
    for (struct pg_widget *c = w->first; c; c = c->next)
        if (c->visible)
            return c;
    return NULL;
}

static void stack_measure(struct pg_widget *w, int *width, int *height)
{
    struct pg_widget *c = stack_current(w);
    pg_widget_measure(c, width, height);
}

static int stack_hfw(struct pg_widget *w, int width)
{
    struct pg_widget *c = stack_current(w);
    return c ? pg_widget_height_for_width(c, width) : 0;
}

static void stack_arrange(struct pg_widget *w)
{
    struct pg_widget *c = stack_current(w);
    if (c)
        pg_widget_place(c, w->rect.x, w->rect.y, w->rect.w, w->rect.h);
}

static const struct pg_widget_class stack_class = {
    .name = "stack",
    .size = sizeof(struct stack),
    .measure = stack_measure,
    .height_for_width = stack_hfw,
    .arrange = stack_arrange,
};

struct pg_widget *pg_stack_new(void)
{
    struct pg_widget *w = pg_widget_new(&stack_class);
    if (w)
        w->clip = true;
    return w;
}

void pg_stack_set_index(struct pg_widget *w, int index)
{
    if (!w || w->cls != &stack_class)
        return;
    struct stack *s = (struct stack *)w;
    if (s->index == index)
        return;
    s->index = index;
    stack_sync(w);
    pg_widget_queue_layout(w);
    pg_widget_queue_draw(w);
}

int pg_stack_index(const struct pg_widget *w)
{
    return w && w->cls == &stack_class ? ((const struct stack *)w)->index : -1;
}

struct scroll {
    struct pg_widget base;
    int offset;
    int content_h;
    bool dragging;
    int drag_y, drag_offset;
};

#define BAR_ZONE 12

static void scroll_measure(struct pg_widget *w, int *width, int *height)
{
    int cw = 0, chh = 0;
    pg_widget_measure(w->first, &cw, &chh);
    *width = cw + BAR_ZONE;
    *height = pg__min(chh, 400);
}

static int scroll_hfw(struct pg_widget *w, int width)
{
    (void)width;
    int cw, chh;
    scroll_measure(w, &cw, &chh);
    return chh;
}

static int max_offset(struct scroll *s)
{
    return pg__max(0, s->content_h - s->base.rect.h);
}

static void scroll_arrange(struct pg_widget *w)
{
    struct scroll *s = (struct scroll *)w;
    struct pg_widget *c = w->first;
    if (!c)
        return;
    int cw = w->rect.w;
    s->content_h = pg_widget_height_for_width(c, cw);
    if (s->content_h > w->rect.h) {
        cw = pg__max(0, w->rect.w - BAR_ZONE);
        s->content_h = pg_widget_height_for_width(c, cw);
    }
    s->offset = pg__clamp(s->offset, 0, max_offset(s));
    pg_widget_place(c, w->rect.x, w->rect.y - s->offset, cw, pg__max(s->content_h, w->rect.h));
}

static struct pg_rect thumb(struct scroll *s)
{
    struct pg_rect r = s->base.rect;
    int h = r.h <= 0 || s->content_h <= 0 ? 0 : pg__max(28, r.h * r.h / s->content_h);
    int span = r.h - h, mo = max_offset(s);
    int y = r.y + (mo ? span * s->offset / mo : 0);
    struct pg_rect t = {r.x + r.w - 9, y + 2, 6, pg__max(0, h - 4)};
    return t;
}

static void scroll_draw_after(struct pg_widget *w, struct pg_canvas *c)
{
    struct scroll *s = (struct scroll *)w;
    if (s->content_h <= w->rect.h)
        return;
    struct pg_rect t = thumb(s);
    const struct pg_theme *th = pg_theme();
    bool hot = s->dragging || w->hovered;
    pgc_rrect(c, t.x, t.y, t.w, t.h, 3, hot ? th->text3 : th->surface3, hot ? 220 : 180);
}

static void scroll_set(struct scroll *s, int offset)
{
    offset = pg__clamp(offset, 0, max_offset(s));
    if (offset == s->offset)
        return;
    s->offset = offset;
    scroll_arrange(&s->base);
    pg_widget_queue_draw(&s->base);
}

static bool scroll_event(struct pg_widget *w, const struct pg_event *e)
{
    struct scroll *s = (struct scroll *)w;
    switch (e->type) {
    case PG_EVENT_WHEEL: {
        if (s->content_h <= w->rect.h)
            return false;
        int before = s->offset;
        scroll_set(s, s->offset - e->wheel * 48);
        (void)before;
        return true;
    }
    case PG_EVENT_POINTER_DOWN:
        if (e->button == PG_BUTTON_LEFT && s->content_h > w->rect.h && e->x >= w->rect.x + w->rect.w - BAR_ZONE) {
            struct pg_rect t = thumb(s);
            if (e->y < t.y || e->y >= t.y + t.h) {
                int span = w->rect.h - t.h;
                if (span > 0)
                    scroll_set(s, (e->y - w->rect.y - t.h / 2) * max_offset(s) / span);
            }
            s->dragging = true;
            s->drag_y = e->y;
            s->drag_offset = s->offset;
            pg_widget_queue_draw(w);
            return true;
        }
        return false;
    case PG_EVENT_POINTER_MOVE:
        if (s->dragging) {
            struct pg_rect t = thumb(s);
            int span = w->rect.h - t.h - 4;
            if (span > 0)
                scroll_set(s, s->drag_offset + (e->y - s->drag_y) * max_offset(s) / span);
            return true;
        }
        if (s->content_h > w->rect.h)
            pg_widget_queue_draw(w);
        return false;
    case PG_EVENT_POINTER_UP:
        if (s->dragging) {
            s->dragging = false;
            pg_widget_queue_draw(w);
            return true;
        }
        return false;
    case PG_EVENT_KEY_DOWN:
        if (e->key == PG_KEY_PAGE_DOWN) {
            scroll_set(s, s->offset + w->rect.h * 9 / 10);
            return true;
        }
        if (e->key == PG_KEY_PAGE_UP) {
            scroll_set(s, s->offset - w->rect.h * 9 / 10);
            return true;
        }
        return false;
    case PG_EVENT_ENTER:
    case PG_EVENT_LEAVE:
        if (s->content_h > w->rect.h)
            pg_widget_queue_draw(w);
        return false;
    default:
        return false;
    }
}

const struct pg_widget_class pg__scroll_class = {
    .name = "scroll",
    .size = sizeof(struct scroll),
    .measure = scroll_measure,
    .height_for_width = scroll_hfw,
    .arrange = scroll_arrange,
    .draw_after = scroll_draw_after,
    .event = scroll_event,
};

struct pg_widget *pg_scroll_new(void)
{
    struct pg_widget *w = pg_widget_new(&pg__scroll_class);
    if (w) {
        w->clip = true;
        w->vexpand = true;
    }
    return w;
}

void pg_scroll_to(struct pg_widget *w, int y)
{
    if (w && w->cls == &pg__scroll_class)
        scroll_set((struct scroll *)w, y);
}

int pg_scroll_offset(const struct pg_widget *w)
{
    return w && w->cls == &pg__scroll_class ? ((const struct scroll *)w)->offset : 0;
}

static void reveal(struct pg_widget *w, struct pg_rect r)
{
    struct scroll *s = (struct scroll *)w;
    int top = r.y - 8, bottom = r.y + r.h + 8;
    if (top < w->rect.y)
        scroll_set(s, s->offset - (w->rect.y - top));
    else if (bottom > w->rect.y + w->rect.h)
        scroll_set(s, s->offset + (bottom - w->rect.y - w->rect.h));
}

void pg_scroll_reveal(struct pg_widget *w, const struct pg_widget *child)
{
    if (w && w->cls == &pg__scroll_class && child)
        reveal(w, child->rect);
}

void pg__scroll_reveal_rect(struct pg_widget *from, struct pg_rect r)
{
    for (struct pg_widget *p = from ? from->parent : NULL; p; p = p->parent)
        if (p->cls == &pg__scroll_class)
            reveal(p, r);
}

void pg__scroll_reveal_ancestors(struct pg_widget *w)
{
    for (struct pg_widget *p = w ? w->parent : NULL; p; p = p->parent)
        if (p->cls == &pg__scroll_class)
            pg_scroll_reveal(p, w);
}
