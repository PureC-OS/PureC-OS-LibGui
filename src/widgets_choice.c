#include "internal.h"

static char **copy_labels(const char *const *labels, int count)
{
    if (count <= 0)
        return NULL;
    char **out = calloc((size_t)count, sizeof(char *));
    if (!out)
        return NULL;
    for (int i = 0; i < count; i++)
        out[i] = pg__strdup(labels && labels[i] ? labels[i] : "");
    return out;
}

static void free_labels(char **labels, int count)
{
    for (int i = 0; labels && i < count; i++)
        free(labels[i]);
    free(labels);
}

struct segmented {
    struct pg_widget base;
    char **labels;
    int count;
    int selected;
    int hot;
    int down;
};

static void seg_measure(struct pg_widget *w, int *width, int *height)
{
    struct segmented *s = (struct segmented *)w;
    int best = 0;
    for (int i = 0; i < s->count; i++)
        best = pg__max(best, pgc_text_w(PGF_MEDIUM, s->labels[i]) + 28);
    *width = best * s->count + 6;
    *height = 36;
}

static int seg_at(struct segmented *s, int x)
{
    int seg = (s->base.rect.w - 6) / (s->count ? s->count : 1);
    if (seg <= 0)
        return -1;
    int i = (x - s->base.rect.x - 3) / seg;
    return i >= 0 && i < s->count ? i : -1;
}

static void seg_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct segmented *s = (struct segmented *)w;
    const struct pg_theme *p = pg_theme();
    struct pg_rect r = w->rect;
    pgc_rrect(c, r.x, r.y, r.w, r.h, 9, p->surface2, 255);
    int seg = (r.w - 6) / (s->count ? s->count : 1);
    for (int i = 0; i < s->count; i++) {
        int sx = r.x + 3 + i * seg;
        bool sel = s->selected == i;
        if (sel)
            pgc_rrect(c, sx, r.y + 3, seg, r.h - 6, 7, p->accent, 255);
        else if (s->hot == i && w->hovered)
            pgc_rrect(c, sx, r.y + 3, seg, r.h - 6, 7, p->surface3, 255);
        pgc_text_center(c, PGF_MEDIUM, sx, r.y + (r.h - pgc_font_h(PGF_MEDIUM)) / 2, seg, s->labels[i],
                        sel ? p->on_accent : p->text2);
    }
    pg_widget_draw_focus(w, c, 9);
}

static void seg_select(struct segmented *s, int i)
{
    if (i < 0 || i >= s->count || i == s->selected)
        return;
    s->selected = i;
    pg_widget_queue_draw(&s->base);
    pg_widget_emit(&s->base, PG_SIGNAL_CHANGED);
}

static bool seg_event(struct pg_widget *w, const struct pg_event *e)
{
    struct segmented *s = (struct segmented *)w;
    switch (e->type) {
    case PG_EVENT_POINTER_MOVE: {
        int h = seg_at(s, e->x);
        if (h != s->hot) {
            s->hot = h;
            pg_widget_queue_draw(w);
        }
        return false;
    }
    case PG_EVENT_LEAVE:
        s->hot = -1;
        return false;
    case PG_EVENT_POINTER_DOWN:
        if (e->button != PG_BUTTON_LEFT)
            return false;
        s->down = seg_at(s, e->x);
        return true;
    case PG_EVENT_POINTER_UP:
        if (pg__rect_has(w->rect, e->x, e->y) && seg_at(s, e->x) == s->down)
            seg_select(s, s->down);
        return true;
    case PG_EVENT_KEY_DOWN:
        if (e->key == PG_KEY_LEFT)
            seg_select(s, s->selected - 1);
        else if (e->key == PG_KEY_RIGHT)
            seg_select(s, s->selected + 1);
        else
            return false;
        return true;
    default:
        return false;
    }
}

static void seg_destroy(struct pg_widget *w)
{
    struct segmented *s = (struct segmented *)w;
    free_labels(s->labels, s->count);
}

static const struct pg_widget_class segmented_class = {
    .name = "segmented",
    .size = sizeof(struct segmented),
    .measure = seg_measure,
    .draw = seg_draw,
    .event = seg_event,
    .destroy = seg_destroy,
};

struct pg_widget *pg_segmented_new(const char *const *labels, int count)
{
    struct segmented *s = (struct segmented *)pg_widget_new(&segmented_class);
    if (!s)
        return NULL;
    s->labels = copy_labels(labels, count);
    s->count = s->labels ? count : 0;
    s->hot = -1;
    s->down = -1;
    s->base.focusable = true;
    return &s->base;
}

int pg_segmented_selected(const struct pg_widget *w)
{
    return w && w->cls == &segmented_class ? ((const struct segmented *)w)->selected : -1;
}

void pg_segmented_set_selected(struct pg_widget *w, int index)
{
    if (!w || w->cls != &segmented_class)
        return;
    struct segmented *s = (struct segmented *)w;
    if (index >= 0 && index < s->count) {
        s->selected = index;
        pg_widget_queue_draw(w);
    }
}

struct tabs {
    struct pg_widget base;
    char **labels;
    int count;
    int selected;
    int hot;
    int down;
    struct pg_widget *stack;
};

#define TAB_H 40

static int tab_w(struct tabs *t, int i)
{
    return pgc_text_w(PGF_MEDIUM, t->labels[i]) + 28;
}

static void tabs_measure(struct pg_widget *w, int *width, int *height)
{
    struct tabs *t = (struct tabs *)w;
    int sum = 0;
    for (int i = 0; i < t->count; i++)
        sum += tab_w(t, i) + 4;
    *width = sum;
    *height = TAB_H;
}

static int tab_at(struct tabs *t, int x, int y)
{
    if (y < t->base.rect.y || y >= t->base.rect.y + TAB_H - 2)
        return -1;
    int tx = t->base.rect.x;
    for (int i = 0; i < t->count; i++) {
        int tw = tab_w(t, i);
        if (x >= tx && x < tx + tw)
            return i;
        tx += tw + 4;
    }
    return -1;
}

static void tabs_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct tabs *t = (struct tabs *)w;
    const struct pg_theme *p = pg_theme();
    int tx = w->rect.x, y = w->rect.y;
    for (int i = 0; i < t->count; i++) {
        int tw = tab_w(t, i);
        bool sel = t->selected == i;
        if (!sel && t->hot == i && w->hovered)
            pgc_rrect(c, tx, y + 2, tw, 32, 8, p->surface2, 255);
        pgc_text_center(c, PGF_MEDIUM, tx, y + 9, tw, t->labels[i], sel ? p->text : p->text3);
        if (sel) {
            pgc_rrect(c, tx + 10, y + 35, tw - 20, 3, 1, p->accent, 255);
            if (pg_widget_focus_visible(w))
                pgc_rrect_stroke(c, tx, y + 2, tw, 32, 8, p->accent, 170);
        }
        tx += tw + 4;
    }
    pgc_fill_a(c, w->rect.x, y + TAB_H - 2, w->rect.w, 1, p->border, 140);
}

static void tabs_select(struct tabs *t, int i)
{
    if (i < 0 || i >= t->count || i == t->selected)
        return;
    t->selected = i;
    if (t->stack)
        pg_stack_set_index(t->stack, i);
    pg_widget_queue_draw(&t->base);
    pg_widget_emit(&t->base, PG_SIGNAL_CHANGED);
}

static bool tabs_event(struct pg_widget *w, const struct pg_event *e)
{
    struct tabs *t = (struct tabs *)w;
    switch (e->type) {
    case PG_EVENT_POINTER_MOVE: {
        int h = tab_at(t, e->x, e->y);
        if (h != t->hot) {
            t->hot = h;
            pg_widget_queue_draw(w);
        }
        return false;
    }
    case PG_EVENT_LEAVE:
        t->hot = -1;
        return false;
    case PG_EVENT_POINTER_DOWN:
        if (e->button != PG_BUTTON_LEFT)
            return false;
        t->down = tab_at(t, e->x, e->y);
        return t->down >= 0;
    case PG_EVENT_POINTER_UP:
        if (tab_at(t, e->x, e->y) == t->down)
            tabs_select(t, t->down);
        return true;
    case PG_EVENT_KEY_DOWN:
        if (e->key == PG_KEY_LEFT)
            tabs_select(t, t->selected - 1);
        else if (e->key == PG_KEY_RIGHT)
            tabs_select(t, t->selected + 1);
        else
            return false;
        return true;
    default:
        return false;
    }
}

static void tabs_destroy(struct pg_widget *w)
{
    struct tabs *t = (struct tabs *)w;
    free_labels(t->labels, t->count);
}

static const struct pg_widget_class tabs_class = {
    .name = "tabs",
    .size = sizeof(struct tabs),
    .measure = tabs_measure,
    .draw = tabs_draw,
    .event = tabs_event,
    .destroy = tabs_destroy,
};

struct pg_widget *pg_tabs_new(const char *const *labels, int count)
{
    struct tabs *t = (struct tabs *)pg_widget_new(&tabs_class);
    if (!t)
        return NULL;
    t->labels = copy_labels(labels, count);
    t->count = t->labels ? count : 0;
    t->hot = -1;
    t->down = -1;
    t->base.focusable = true;
    return &t->base;
}

int pg_tabs_selected(const struct pg_widget *w)
{
    return w && w->cls == &tabs_class ? ((const struct tabs *)w)->selected : -1;
}

void pg_tabs_set_selected(struct pg_widget *w, int index)
{
    if (!w || w->cls != &tabs_class)
        return;
    struct tabs *t = (struct tabs *)w;
    if (index < 0 || index >= t->count)
        return;
    t->selected = index;
    if (t->stack)
        pg_stack_set_index(t->stack, index);
    pg_widget_queue_draw(w);
}

void pg_tabs_bind_stack(struct pg_widget *w, struct pg_widget *stack)
{
    if (!w || w->cls != &tabs_class)
        return;
    struct tabs *t = (struct tabs *)w;
    t->stack = stack;
    if (stack)
        pg_stack_set_index(stack, t->selected);
}

struct item {
    char *label;
    int icon;
    bool enabled;
    bool separator;
    pg_menu_fn fn;
    void *data;
};

struct popup_list {
    struct pg_widget base;
    struct item *items;
    int count;
    int checked;
    int hot;
    int down;
    void (*pick)(struct popup_list *pl, int index);
    void *owner;
};

#define ITEM_H 34
#define SEP_H 9

static int item_h(const struct item *it)
{
    return it->separator ? SEP_H : ITEM_H;
}

static void plist_measure(struct pg_widget *w, int *width, int *height)
{
    struct popup_list *pl = (struct popup_list *)w;
    int best = 120, h = 8;
    for (int i = 0; i < pl->count; i++) {
        h += item_h(&pl->items[i]);
        if (!pl->items[i].separator)
            best = pg__max(best, pgc_text_w(PGF_MEDIUM, pl->items[i].label) + 60 + (pl->items[i].icon >= 0 ? 28 : 0));
    }
    *width = best;
    *height = h;
}

static int plist_at(struct popup_list *pl, int y)
{
    int ry = pl->base.rect.y + 4;
    for (int i = 0; i < pl->count; i++) {
        int h = item_h(&pl->items[i]);
        if (y >= ry && y < ry + h)
            return pl->items[i].separator || !pl->items[i].enabled ? -1 : i;
        ry += h;
    }
    return -1;
}

static void plist_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct popup_list *pl = (struct popup_list *)w;
    const struct pg_theme *p = pg_theme();
    int x = w->rect.x, ry = w->rect.y + 4, ww = w->rect.w;
    for (int i = 0; i < pl->count; i++) {
        struct item *it = &pl->items[i];
        int h = item_h(it);
        if (it->separator) {
            pgc_fill_a(c, x + 10, ry + h / 2, ww - 20, 1, p->border, 160);
            ry += h;
            continue;
        }
        if (pl->hot == i)
            pgc_rrect(c, x + 4, ry, ww - 8, h, 7, p->accent, p->light ? 36 : 44);
        bool sel = pl->checked == i;
        uint32_t fg = !it->enabled ? p->text3 : sel ? p->text : p->text2;
        int tx = x + 14;
        if (it->icon >= 0) {
            pgc_icon(c, it->icon, PG_ICON_S, tx, ry + (h - pgc_icon_px(PG_ICON_S)) / 2, fg);
            tx += 28;
        }
        pgc_text_fit(c, sel ? PGF_MEDIUM : PGF_BODY, tx, ry + (h - pgc_font_h(PGF_BODY)) / 2, x + ww - 40 - tx,
                     it->label, fg);
        if (sel)
            pgc_icon(c, PGI_CHECK, PG_ICON_S, x + ww - 30, ry + (h - pgc_icon_px(PG_ICON_S)) / 2, p->accent);
        ry += h;
    }
}

static void plist_set_hot(struct popup_list *pl, int h)
{
    if (h == pl->hot)
        return;
    pl->hot = h;
    pg_widget_queue_draw(&pl->base);
}

static void plist_step(struct popup_list *pl, int dir)
{
    int i = pl->hot;
    for (int n = 0; n < pl->count; n++) {
        i = (i + dir + pl->count) % pl->count;
        if (i < 0)
            i = 0;
        if (!pl->items[i].separator && pl->items[i].enabled) {
            plist_set_hot(pl, i);
            return;
        }
    }
}

static bool plist_event(struct pg_widget *w, const struct pg_event *e)
{
    struct popup_list *pl = (struct popup_list *)w;
    switch (e->type) {
    case PG_EVENT_POINTER_MOVE:
        plist_set_hot(pl, plist_at(pl, e->y));
        return true;
    case PG_EVENT_LEAVE:
        plist_set_hot(pl, -1);
        return false;
    case PG_EVENT_POINTER_DOWN:
        pl->down = plist_at(pl, e->y);
        return true;
    case PG_EVENT_POINTER_UP: {
        int i = plist_at(pl, e->y);
        if (i >= 0 && i == pl->down && pl->pick)
            pl->pick(pl, i);
        return true;
    }
    case PG_EVENT_KEY_DOWN:
        if (e->key == PG_KEY_DOWN)
            plist_step(pl, 1);
        else if (e->key == PG_KEY_UP)
            plist_step(pl, -1);
        else if ((e->key == PG_KEY_ENTER || e->key == PG_KEY_KP_ENTER || e->key == PG_KEY_SPACE) && pl->hot >= 0 && pl->pick)
            pl->pick(pl, pl->hot);
        else
            return false;
        return true;
    default:
        return false;
    }
}

static const struct pg_widget_class plist_class = {
    .name = "popup-list",
    .size = sizeof(struct popup_list),
    .measure = plist_measure,
    .draw = plist_draw,
    .event = plist_event,
};

static struct pg_window *open_popup(struct pg_window *owner, int sx, int sy, int min_w, int above_y,
                                    struct item *items, int count, int checked,
                                    void (*pick)(struct popup_list *, int), void *who,
                                    void (*done)(struct pg_window *, void *), struct popup_list **out)
{
    struct popup_list *pl = (struct popup_list *)pg_widget_new(&plist_class);
    if (!pl)
        return NULL;
    pl->items = items;
    pl->count = count;
    pl->checked = checked;
    pl->hot = checked;
    pl->down = -1;
    pl->pick = pick;
    pl->owner = who;
    int mw, mh;
    pg_widget_measure(&pl->base, &mw, &mh);
    mw = pg__max(mw, min_w);
    int sw = 0, sh = 0;
    pg_screen_size(&sw, &sh);
    if (sh > 0 && sy + mh > sh && above_y - mh >= 0)
        sy = above_y - mh;
    struct pg_window *pop = pg__popup_new(owner, sx, sy, mw, mh, done, who);
    if (!pop) {
        pl->items = NULL;
        pg_widget_destroy(&pl->base);
        return NULL;
    }
    pg_window_set_background(pop, pg_theme()->light ? 0xffffff : pg_theme()->surface);
    pg_window_set_child(pop, &pl->base);
    pg_window_set_focus(pop, &pl->base);
    pg_window_raise(pop);
    if (out)
        *out = pl;
    return pop;
}

struct dropdown {
    struct pg_widget base;
    struct item *items;
    int count;
    int selected;
    struct pg_window *popup;
};

static void dd_measure(struct pg_widget *w, int *width, int *height)
{
    struct dropdown *d = (struct dropdown *)w;
    int best = 80;
    for (int i = 0; i < d->count; i++)
        best = pg__max(best, pgc_text_w(PGF_BODY, d->items[i].label));
    *width = best + 56;
    *height = 38;
}

static void dd_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct dropdown *d = (struct dropdown *)w;
    const struct pg_theme *p = pg_theme();
    struct pg_rect r = w->rect;
    bool hot = w->hovered && pg_widget_is_enabled(w);
    bool open = d->popup != NULL;
    pgc_rrect(c, r.x, r.y, r.w, r.h, 8, p->light ? 0xffffff : (hot ? p->surface2 : p->bg), 255);
    pgc_rrect_stroke(c, r.x, r.y, r.w, r.h, 8, open ? p->accent : (hot ? p->text3 : p->border), 255);
    const char *cur = d->selected >= 0 && d->selected < d->count ? d->items[d->selected].label : "";
    uint32_t fg = pg_widget_is_enabled(w) ? p->text : p->text3;
    pgc_text_fit(c, PGF_BODY, r.x + 12, r.y + (r.h - pgc_font_h(PGF_BODY)) / 2, r.w - 44, cur, fg);
    float cx = (float)(r.x + r.w - 20), cy = (float)(r.y + r.h / 2);
    float dy = open ? -2.0f : 2.0f;
    pgc_line(c, cx - 5.0f, cy - dy, cx, cy + dy, 1.6f, p->text3);
    pgc_line(c, cx, cy + dy, cx + 5.0f, cy - dy, 1.6f, p->text3);
    pg_widget_draw_focus(w, c, 8);
}

static void dd_done(struct pg_window *pop, void *data)
{
    (void)pop;
    struct dropdown *d = data;
    d->popup = NULL;
    pg_widget_queue_draw(&d->base);
}

static void dd_pick(struct popup_list *pl, int index)
{
    struct dropdown *d = pl->owner;
    if (d->popup)
        pg_window_destroy(d->popup);
    if (index != d->selected) {
        d->selected = index;
        pg_widget_queue_draw(&d->base);
        pg_widget_emit(&d->base, PG_SIGNAL_CHANGED);
    }
}

static void dd_open(struct dropdown *d)
{
    struct pg_widget *w = &d->base;
    if (d->popup) {
        pg_window_destroy(d->popup);
        return;
    }
    if (!w->window || !d->count)
        return;
    int wx, wy;
    pg_window_screen_position(w->window, &wx, &wy);
    d->popup = open_popup(w->window, wx + w->rect.x, wy + w->rect.y + w->rect.h + 4, w->rect.w, wy + w->rect.y - 4,
                          d->items, d->count, d->selected, dd_pick, d, dd_done, NULL);
    pg_widget_queue_draw(w);
}

static bool dd_event(struct pg_widget *w, const struct pg_event *e)
{
    struct dropdown *d = (struct dropdown *)w;
    switch (e->type) {
    case PG_EVENT_POINTER_DOWN:
        if (e->button != PG_BUTTON_LEFT)
            return false;
        dd_open(d);
        return true;
    case PG_EVENT_POINTER_UP:
        return true;
    case PG_EVENT_KEY_DOWN:
        if (e->key == PG_KEY_SPACE || e->key == PG_KEY_ENTER) {
            dd_open(d);
            return true;
        }
        if (e->key == PG_KEY_UP || e->key == PG_KEY_DOWN) {
            int n = pg__clamp(d->selected + (e->key == PG_KEY_DOWN ? 1 : -1), 0, d->count - 1);
            if (n != d->selected) {
                d->selected = n;
                pg_widget_queue_draw(w);
                pg_widget_emit(w, PG_SIGNAL_CHANGED);
            }
            return true;
        }
        return false;
    default:
        return false;
    }
}

static void free_items(struct item *items, int count)
{
    for (int i = 0; items && i < count; i++)
        free(items[i].label);
    free(items);
}

static void dd_destroy(struct pg_widget *w)
{
    struct dropdown *d = (struct dropdown *)w;
    if (d->popup)
        pg_window_destroy(d->popup);
    free_items(d->items, d->count);
}

static const struct pg_widget_class dropdown_class = {
    .name = "dropdown",
    .size = sizeof(struct dropdown),
    .measure = dd_measure,
    .draw = dd_draw,
    .event = dd_event,
    .destroy = dd_destroy,
};

void pg_dropdown_set_items(struct pg_widget *w, const char *const *labels, int count)
{
    if (!w || w->cls != &dropdown_class)
        return;
    struct dropdown *d = (struct dropdown *)w;
    if (d->popup)
        pg_window_destroy(d->popup);
    free_items(d->items, d->count);
    d->items = count > 0 ? calloc((size_t)count, sizeof(struct item)) : NULL;
    d->count = d->items ? count : 0;
    for (int i = 0; i < d->count; i++) {
        d->items[i].label = pg__strdup(labels ? labels[i] : "");
        d->items[i].icon = PG_NO_ICON;
        d->items[i].enabled = true;
    }
    d->selected = pg__clamp(d->selected, 0, pg__max(0, d->count - 1));
    pg_widget_queue_layout(w);
    pg_widget_queue_draw(w);
}

struct pg_widget *pg_dropdown_new(const char *const *labels, int count)
{
    struct dropdown *d = (struct dropdown *)pg_widget_new(&dropdown_class);
    if (!d)
        return NULL;
    d->base.focusable = true;
    pg_dropdown_set_items(&d->base, labels, count);
    return &d->base;
}

int pg_dropdown_selected(const struct pg_widget *w)
{
    return w && w->cls == &dropdown_class ? ((const struct dropdown *)w)->selected : -1;
}

void pg_dropdown_set_selected(struct pg_widget *w, int index)
{
    if (!w || w->cls != &dropdown_class)
        return;
    struct dropdown *d = (struct dropdown *)w;
    if (index >= 0 && index < d->count) {
        d->selected = index;
        pg_widget_queue_draw(w);
    }
}

const char *pg_dropdown_text(const struct pg_widget *w)
{
    if (!w || w->cls != &dropdown_class)
        return "";
    const struct dropdown *d = (const struct dropdown *)w;
    return d->selected >= 0 && d->selected < d->count ? d->items[d->selected].label : "";
}

struct pg_menu {
    struct item *items;
    int count;
    struct pg_window *popup;
};

struct pg_menu *pg_menu_new(void)
{
    return calloc(1, sizeof(struct pg_menu));
}

static int menu_push(struct pg_menu *m, const char *label, int icon, bool sep, pg_menu_fn fn, void *data)
{
    if (!m || m->popup)
        return -1;
    struct item *n = realloc(m->items, sizeof(struct item) * (size_t)(m->count + 1));
    if (!n)
        return -1;
    m->items = n;
    struct item *it = &m->items[m->count];
    memset(it, 0, sizeof(*it));
    it->label = pg__strdup(label);
    it->icon = icon;
    it->enabled = !sep;
    it->separator = sep;
    it->fn = fn;
    it->data = data;
    return m->count++;
}

int pg_menu_add(struct pg_menu *m, const char *label, int icon, pg_menu_fn fn, void *data)
{
    return menu_push(m, label, icon, false, fn, data);
}

void pg_menu_add_separator(struct pg_menu *m)
{
    menu_push(m, "", PG_NO_ICON, true, NULL, NULL);
}

void pg_menu_set_enabled(struct pg_menu *m, int index, bool enabled)
{
    if (m && index >= 0 && index < m->count && !m->items[index].separator)
        m->items[index].enabled = enabled;
}

static void menu_done(struct pg_window *pop, void *data)
{
    (void)pop;
    ((struct pg_menu *)data)->popup = NULL;
}

static void menu_pick(struct popup_list *pl, int index)
{
    struct pg_menu *m = pl->owner;
    pg_menu_fn fn = m->items[index].fn;
    void *data = m->items[index].data;
    if (m->popup)
        pg_window_destroy(m->popup);
    if (fn)
        fn(index, data);
}

void pg_menu_popup(struct pg_menu *m, struct pg_window *owner, int x, int y)
{
    if (!m || !owner || !m->count)
        return;
    if (m->popup)
        pg_window_destroy(m->popup);
    int wx, wy;
    pg_window_screen_position(owner, &wx, &wy);
    m->popup = open_popup(owner, wx + x, wy + y, 160, wy + y, m->items, m->count, -1, menu_pick, m, menu_done, NULL);
}

void pg_menu_destroy(struct pg_menu *m)
{
    if (!m)
        return;
    if (m->popup)
        pg_window_destroy(m->popup);
    free_items(m->items, m->count);
    free(m);
}

struct row {
    int icon;
    char *title;
    char *subtitle;
    char *trailing;
};

struct list {
    struct pg_widget base;
    struct row *rows;
    int count;
    int selected;
    int hot;
    int down;
    int activated;
    bool sidebar;
    struct pg_widget *stack;
};

static int row_h(const struct list *l, const struct row *r)
{
    return (r->subtitle && r->subtitle[0]) ? 54 : (l->sidebar ? 40 : 42);
}

static void list_measure(struct pg_widget *w, int *width, int *height)
{
    struct list *l = (struct list *)w;
    int best = 120, h = 0;
    for (int i = 0; i < l->count; i++) {
        struct row *r = &l->rows[i];
        int tw = pgc_text_w(PGF_MEDIUM, r->title);
        if (r->subtitle)
            tw = pg__max(tw, pgc_text_w(PGF_SMALL, r->subtitle));
        tw += 28 + (r->icon >= 0 ? 34 : 0) + (r->trailing ? pgc_text_w(PGF_SMALL, r->trailing) + 16 : 0);
        best = pg__max(best, tw);
        h += row_h(l, r) + 2;
    }
    *width = best;
    *height = h;
}

static struct pg_rect row_rect(struct list *l, int index)
{
    int y = l->base.rect.y;
    for (int i = 0; i < index; i++)
        y += row_h(l, &l->rows[i]) + 2;
    struct pg_rect r = {l->base.rect.x, y, l->base.rect.w, index < l->count ? row_h(l, &l->rows[index]) : 0};
    return r;
}

static int list_at(struct list *l, int y)
{
    int ry = l->base.rect.y;
    for (int i = 0; i < l->count; i++) {
        int h = row_h(l, &l->rows[i]);
        if (y >= ry && y < ry + h)
            return i;
        ry += h + 2;
    }
    return -1;
}

static void list_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct list *l = (struct list *)w;
    const struct pg_theme *p = pg_theme();
    int clip_top = c->cy0, clip_bottom = c->cy1;
    int y = w->rect.y;
    for (int i = 0; i < l->count; i++) {
        struct row *r = &l->rows[i];
        int h = row_h(l, r);
        if (y + h < clip_top || y > clip_bottom) {
            y += h + 2;
            continue;
        }
        int x = w->rect.x, rw = w->rect.w;
        bool sel = i == l->selected;
        if (sel)
            pgc_rrect(c, x, y, rw, h, 10, p->accent, l->sidebar ? (p->light ? 50 : 60) : (p->light ? 36 : 40));
        else if (i == l->hot && w->hovered)
            pgc_rrect(c, x, y, rw, h, 10, p->surface2, 255);
        if (sel && pg_widget_focus_visible(w))
            pgc_rrect_stroke(c, x, y, rw, h, 10, p->accent, 170);
        int tx = x + 12;
        if (r->icon >= 0) {
            int px = pgc_icon_px(PG_ICON_S);
            pgc_icon(c, r->icon, PG_ICON_S, tx, y + (h - px) / 2, sel ? p->accent : p->text2);
            tx += 34;
        }
        int right = x + rw - 12;
        if (r->trailing && r->trailing[0]) {
            int trw = pgc_text_w(PGF_SMALL, r->trailing);
            pgc_text(c, PGF_SMALL, right - trw, y + (h - pgc_font_h(PGF_SMALL)) / 2, r->trailing, p->text3);
            right -= trw + 12;
        }
        if (r->subtitle && r->subtitle[0]) {
            pgc_text_fit(c, PGF_MEDIUM, tx, y + 9, right - tx, r->title, p->text);
            pgc_text_fit(c, PGF_SMALL, tx, y + 30, right - tx, r->subtitle, p->text3);
        } else {
            pgc_text_fit(c, sel ? PGF_MEDIUM : PGF_BODY, tx, y + (h - pgc_font_h(PGF_BODY)) / 2, right - tx, r->title,
                         sel ? p->text : p->text2);
        }
        y += h + 2;
    }
}

extern void pg__scroll_reveal_rect(struct pg_widget *from, struct pg_rect r);

static void list_select(struct list *l, int i, bool emit)
{
    if (i < 0 || i >= l->count)
        return;
    if (i != l->selected) {
        l->selected = i;
        if (l->stack)
            pg_stack_set_index(l->stack, i);
        pg_widget_queue_draw(&l->base);
        if (emit)
            pg_widget_emit(&l->base, PG_SIGNAL_CHANGED);
    }
    pg__scroll_reveal_rect(&l->base, row_rect(l, i));
}

static bool list_event(struct pg_widget *w, const struct pg_event *e)
{
    struct list *l = (struct list *)w;
    switch (e->type) {
    case PG_EVENT_POINTER_MOVE: {
        int h = list_at(l, e->y);
        if (h != l->hot) {
            l->hot = h;
            pg_widget_queue_draw(w);
        }
        return false;
    }
    case PG_EVENT_LEAVE:
        l->hot = -1;
        return false;
    case PG_EVENT_POINTER_DOWN:
        if (e->button != PG_BUTTON_LEFT)
            return false;
        l->down = list_at(l, e->y);
        return l->down >= 0;
    case PG_EVENT_POINTER_UP: {
        int i = list_at(l, e->y);
        if (i >= 0 && i == l->down) {
            list_select(l, i, true);
            if (e->clicks >= 2) {
                l->activated = i;
                pg_widget_emit(w, PG_SIGNAL_ACTIVATE);
            }
        }
        return true;
    }
    case PG_EVENT_KEY_DOWN:
        if (e->key == PG_KEY_DOWN)
            list_select(l, l->selected + 1, true);
        else if (e->key == PG_KEY_UP)
            list_select(l, l->selected - 1, true);
        else if (e->key == PG_KEY_HOME)
            list_select(l, 0, true);
        else if (e->key == PG_KEY_END)
            list_select(l, l->count - 1, true);
        else if ((e->key == PG_KEY_ENTER || e->key == PG_KEY_KP_ENTER) && l->selected >= 0) {
            l->activated = l->selected;
            pg_widget_emit(w, PG_SIGNAL_ACTIVATE);
        } else
            return false;
        return true;
    default:
        return false;
    }
}

static void free_row(struct row *r)
{
    free(r->title);
    free(r->subtitle);
    free(r->trailing);
}

static void list_destroy(struct pg_widget *w)
{
    struct list *l = (struct list *)w;
    for (int i = 0; i < l->count; i++)
        free_row(&l->rows[i]);
    free(l->rows);
}

static const struct pg_widget_class list_class = {
    .name = "list",
    .size = sizeof(struct list),
    .measure = list_measure,
    .draw = list_draw,
    .event = list_event,
    .destroy = list_destroy,
};

struct pg_widget *pg_list_new(void)
{
    struct list *l = (struct list *)pg_widget_new(&list_class);
    if (!l)
        return NULL;
    l->selected = -1;
    l->hot = -1;
    l->down = -1;
    l->activated = -1;
    l->base.focusable = true;
    return &l->base;
}

static struct list *as_list(const struct pg_widget *w)
{
    return w && w->cls == &list_class ? (struct list *)w : NULL;
}

int pg_list_add(struct pg_widget *w, int icon, const char *title, const char *subtitle)
{
    struct list *l = as_list(w);
    if (!l)
        return -1;
    struct row *n = realloc(l->rows, sizeof(struct row) * (size_t)(l->count + 1));
    if (!n)
        return -1;
    l->rows = n;
    struct row *r = &l->rows[l->count];
    r->icon = icon;
    r->title = pg__strdup(title);
    r->subtitle = subtitle ? pg__strdup(subtitle) : NULL;
    r->trailing = NULL;
    if (l->selected < 0 && l->sidebar)
        l->selected = 0;
    pg_widget_queue_layout(w);
    pg_widget_queue_draw(w);
    return l->count++;
}

void pg_list_clear(struct pg_widget *w)
{
    struct list *l = as_list(w);
    if (!l)
        return;
    for (int i = 0; i < l->count; i++)
        free_row(&l->rows[i]);
    free(l->rows);
    l->rows = NULL;
    l->count = 0;
    l->selected = -1;
    l->hot = -1;
    pg_widget_queue_layout(w);
    pg_widget_queue_draw(w);
}

int pg_list_count(const struct pg_widget *w)
{
    struct list *l = as_list(w);
    return l ? l->count : 0;
}

int pg_list_selected(const struct pg_widget *w)
{
    struct list *l = as_list(w);
    return l ? l->selected : -1;
}

int pg_list_activated(const struct pg_widget *w)
{
    struct list *l = as_list(w);
    return l ? l->activated : -1;
}

void pg_list_set_selected(struct pg_widget *w, int index)
{
    struct list *l = as_list(w);
    if (!l)
        return;
    if (index < 0) {
        l->selected = -1;
        pg_widget_queue_draw(w);
        return;
    }
    list_select(l, index, false);
}

void pg_list_set_item(struct pg_widget *w, int index, int icon, const char *title, const char *subtitle)
{
    struct list *l = as_list(w);
    if (!l || index < 0 || index >= l->count)
        return;
    struct row *r = &l->rows[index];
    r->icon = icon;
    pg__set_text(&r->title, title);
    free(r->subtitle);
    r->subtitle = subtitle ? pg__strdup(subtitle) : NULL;
    pg_widget_queue_layout(w);
    pg_widget_queue_draw(w);
}

void pg_list_set_trailing(struct pg_widget *w, int index, const char *text)
{
    struct list *l = as_list(w);
    if (!l || index < 0 || index >= l->count)
        return;
    struct row *r = &l->rows[index];
    free(r->trailing);
    r->trailing = text ? pg__strdup(text) : NULL;
    pg_widget_queue_draw(w);
}

void pg_list_set_sidebar(struct pg_widget *w, bool sidebar)
{
    struct list *l = as_list(w);
    if (!l)
        return;
    l->sidebar = sidebar;
    if (sidebar && l->selected < 0 && l->count)
        l->selected = 0;
    pg_widget_queue_layout(w);
}

void pg_list_bind_stack(struct pg_widget *w, struct pg_widget *stack)
{
    struct list *l = as_list(w);
    if (!l)
        return;
    l->stack = stack;
    if (stack && l->selected >= 0)
        pg_stack_set_index(stack, l->selected);
}
