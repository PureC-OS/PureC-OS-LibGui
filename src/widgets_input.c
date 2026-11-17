#include "internal.h"

static uint32_t disabled_mix(const struct pg_widget *w, uint32_t color)
{
    return pg_widget_is_enabled(w) ? color : pgc_mix(color, pg_theme()->bg, 120);
}

static bool activate_key(const struct pg_event *e)
{
    return e->type == PG_EVENT_KEY_DOWN && !e->repeat &&
           (e->key == PG_KEY_SPACE || e->key == PG_KEY_ENTER || e->key == PG_KEY_KP_ENTER);
}

static int click_event(struct pg_widget *w, const struct pg_event *e)
{
    if (e->type == PG_EVENT_POINTER_DOWN)
        return e->button == PG_BUTTON_LEFT ? 1 : 0;
    if (e->type == PG_EVENT_POINTER_UP && e->button == PG_BUTTON_LEFT)
        return w->pressed && pg__rect_has(w->rect, e->x, e->y) ? 2 : 1;
    if (activate_key(e))
        return 2;
    return 0;
}

struct button
{
    struct pg_widget base;
    char *label;
    int icon;
    int style;
};

static void button_measure(struct pg_widget *w, int *width, int *height)
{
    struct button *b = (struct button *)w;
    bool text = b->label && b->label[0];
    *width = text ? pgc_text_w(PGF_MEDIUM, b->label) + 32 + (b->icon >= 0 ? 24 : 0) : 38;
    *height = 38;
}

static void button_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct button *b = (struct button *)w;
    const struct pg_theme *p = pg_theme();
    struct pg_rect r = w->rect;
    bool hot = w->hovered && pg_widget_is_enabled(w);
    bool down = w->pressed && hot;
    uint32_t bg, fg;
    switch (b->style)
    {
    case PG_BUTTON_PRIMARY:
        bg = p->accent;
        fg = p->on_accent;
        break;
    case PG_BUTTON_DANGER:
        bg = p->danger;
        fg = pgc_luma(p->danger) > 150 ? 0x11111b : 0xffffff;
        break;
    case PG_BUTTON_GHOST:
        bg = p->surface;
        fg = p->accent;
        break;
    case PG_BUTTON_FLAT:
        bg = p->bg;
        fg = p->text;
        break;
    default:
        bg = p->surface2;
        fg = p->text;
        break;
    }
    bool flat = b->style == PG_BUTTON_FLAT;
    if (hot)
        bg = (b->style == PG_BUTTON_GHOST || flat) ? p->surface2 : pgc_lighten(bg, p->light ? -18 : 22);
    if (down)
        bg = pgc_lighten(bg, p->light ? -30 : -24);
    bg = disabled_mix(w, bg);
    fg = disabled_mix(w, fg);
    if (!flat || hot)
        pgc_rrect(c, r.x, r.y, r.w, r.h, 8, bg, 255);
    if (b->style == PG_BUTTON_SECONDARY)
        pgc_rrect_stroke(c, r.x, r.y, r.w, r.h, 8, p->border, 140);
    int ipx = pgc_icon_px(PG_ICON_S);
    bool text = b->label && b->label[0];
    int tw = (text ? pgc_text_w(PGF_MEDIUM, b->label) : 0) + (b->icon >= 0 ? (text ? 24 : ipx) : 0);
    int tx = r.x + (r.w - tw) / 2;
    if (b->icon >= 0)
    {
        pgc_icon(c, b->icon, PG_ICON_S, tx, r.y + (r.h - ipx) / 2, fg);
        tx += 24;
    }
    if (text)
    {
        struct pg_clip saved = pgc_clip_push(c, r.x + 4, r.y, r.w - 8, r.h);
        pgc_text(c, PGF_MEDIUM, tx, r.y + (r.h - pgc_font_h(PGF_MEDIUM)) / 2, b->label, fg);
        pgc_clip_pop(c, saved);
    }
    pg_widget_draw_focus(w, c, 8);
}

static bool button_event(struct pg_widget *w, const struct pg_event *e)
{
    int r = click_event(w, e);
    if (r == 2)
        pg_widget_emit(w, PG_SIGNAL_CLICKED);
    return r != 0;
}

static void button_destroy(struct pg_widget *w)
{
    free(((struct button *)w)->label);
}

static const struct pg_widget_class button_class = {
    .name = "button",
    .size = sizeof(struct button),
    .measure = button_measure,
    .draw = button_draw,
    .event = button_event,
    .destroy = button_destroy,
};

struct pg_widget *pg_button_new(const char *label, int icon, enum pg_button_style style)
{
    struct button *b = (struct button *)pg_widget_new(&button_class);
    if (!b)
        return NULL;
    b->label = pg__strdup(label);
    b->icon = icon;
    b->style = style;
    b->base.focusable = true;
    return &b->base;
}

void pg_button_set_label(struct pg_widget *w, const char *label)
{
    if (!w || w->cls != &button_class)
        return;
    pg__set_text(&((struct button *)w)->label, label);
    pg_widget_queue_layout(w);
    pg_widget_queue_draw(w);
}

void pg_button_set_icon(struct pg_widget *w, int icon)
{
    if (!w || w->cls != &button_class)
        return;
    ((struct button *)w)->icon = icon;
    pg_widget_queue_layout(w);
    pg_widget_queue_draw(w);
}

void pg_button_set_style(struct pg_widget *w, enum pg_button_style style)
{
    if (!w || w->cls != &button_class)
        return;
    ((struct button *)w)->style = style;
    pg_widget_queue_draw(w);
}

struct toggle
{
    struct pg_widget base;
    bool active;
    int anim;
    uint32_t timer;
};

#define TOGGLE_W 46
#define TOGGLE_H 26

static void toggle_measure(struct pg_widget *w, int *width, int *height)
{
    (void)w;
    *width = TOGGLE_W;
    *height = TOGGLE_H;
}

static void toggle_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct toggle *t = (struct toggle *)w;
    const struct pg_theme *p = pg_theme();
    int x = w->rect.x, y = w->rect.y + (w->rect.h - TOGGLE_H) / 2;
    uint32_t track = pgc_mix(p->surface3, p->accent, (uint32_t)(t->anim * 255 / 256));
    if (w->hovered && pg_widget_is_enabled(w))
        track = pgc_lighten(track, p->light ? -14 : 18);
    track = disabled_mix(w, track);
    pgc_rrect(c, x, y, TOGGLE_W, TOGGLE_H, TOGGLE_H / 2, track, 255);
    int span = TOGGLE_W - TOGGLE_H;
    int kx = x + 3 + span * t->anim / 256;
    uint32_t knob = pgc_mix(0xffffff, p->on_accent, (uint32_t)(t->anim * 255 / 256));
    pgc_circle(c, kx + (TOGGLE_H - 6) / 2, y + TOGGLE_H / 2, (TOGGLE_H - 6) / 2, disabled_mix(w, knob), 255);
    pg_widget_draw_focus(w, c, TOGGLE_H / 2);
}

static bool toggle_tick(void *data)
{
    struct toggle *t = data;
    int target = t->active ? 256 : 0;
    int step = 48;
    if (t->anim < target)
        t->anim = pg__min(target, t->anim + step);
    else if (t->anim > target)
        t->anim = pg__max(target, t->anim - step);
    pg_widget_queue_draw(&t->base);
    if (t->anim == target)
    {
        t->timer = 0;
        return false;
    }
    return true;
}

static void toggle_apply(struct toggle *t, bool active, bool emit)
{
    if (t->active == active)
        return;
    t->active = active;
    if (!t->base.window)
    {
        t->anim = active ? 256 : 0;
    }
    else if (!t->timer)
    {
        t->timer = pg_timer_add(16, toggle_tick, t);
    }
    pg_widget_queue_draw(&t->base);
    if (emit)
        pg_widget_emit(&t->base, PG_SIGNAL_CHANGED);
}

static bool toggle_event(struct pg_widget *w, const struct pg_event *e)
{
    struct toggle *t = (struct toggle *)w;
    int r = click_event(w, e);
    if (r == 2)
        toggle_apply(t, !t->active, true);
    return r != 0;
}

static void toggle_destroy(struct pg_widget *w)
{
    struct toggle *t = (struct toggle *)w;
    if (t->timer)
        pg_timer_remove(t->timer);
}

static const struct pg_widget_class toggle_class = {
    .name = "toggle",
    .size = sizeof(struct toggle),
    .measure = toggle_measure,
    .draw = toggle_draw,
    .event = toggle_event,
    .destroy = toggle_destroy,
};

struct pg_widget *pg_toggle_new(bool active)
{
    struct toggle *t = (struct toggle *)pg_widget_new(&toggle_class);
    if (!t)
        return NULL;
    t->active = active;
    t->anim = active ? 256 : 0;
    t->base.focusable = true;
    t->base.halign = PG_ALIGN_START;
    t->base.valign = PG_ALIGN_CENTER;
    return &t->base;
}

bool pg_toggle_active(const struct pg_widget *w)
{
    return w && w->cls == &toggle_class && ((const struct toggle *)w)->active;
}

void pg_toggle_set_active(struct pg_widget *w, bool active)
{
    if (w && w->cls == &toggle_class)
        toggle_apply((struct toggle *)w, active, false);
}

struct checkbox
{
    struct pg_widget base;
    char *label;
    bool active;
};

static void checkbox_measure(struct pg_widget *w, int *width, int *height)
{
    struct checkbox *k = (struct checkbox *)w;
    *width = 22 + (k->label && k->label[0] ? 10 + pgc_text_w(PGF_BODY, k->label) : 0);
    *height = 24;
}

static void checkbox_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct checkbox *k = (struct checkbox *)w;
    const struct pg_theme *p = pg_theme();
    int x = w->rect.x, y = w->rect.y + (w->rect.h - 22) / 2;
    bool hot = w->hovered && pg_widget_is_enabled(w);
    if (k->active)
    {
        pgc_rrect(c, x, y, 22, 22, 6, disabled_mix(w, hot ? pgc_lighten(p->accent, p->light ? -14 : 18) : p->accent), 255);
        pgc_icon(c, PGI_CHECK, PG_ICON_S, x + 2, y + 2, disabled_mix(w, p->on_accent));
    }
    else
    {
        pgc_rrect(c, x, y, 22, 22, 6, hot ? p->surface2 : (p->light ? 0xffffff : p->bg), 255);
        pgc_rrect_stroke(c, x, y, 22, 22, 6, disabled_mix(w, hot ? p->accent : p->border), 255);
    }
    if (k->label && k->label[0])
        pgc_text_fit(c, PGF_BODY, x + 32, y + (22 - pgc_font_h(PGF_BODY)) / 2, w->rect.w - 32, k->label,
                     disabled_mix(w, p->text));
    if (pg_widget_focus_visible(w))
        pgc_rrect_stroke(c, x - 2, y - 2, 26, 26, 8, p->accent, 170);
}

static bool checkbox_event(struct pg_widget *w, const struct pg_event *e)
{
    struct checkbox *k = (struct checkbox *)w;
    int r = click_event(w, e);
    if (r == 2)
    {
        k->active = !k->active;
        pg_widget_queue_draw(w);
        pg_widget_emit(w, PG_SIGNAL_CHANGED);
    }
    return r != 0;
}

static void checkbox_destroy(struct pg_widget *w)
{
    free(((struct checkbox *)w)->label);
}

static const struct pg_widget_class checkbox_class = {
    .name = "checkbox",
    .size = sizeof(struct checkbox),
    .measure = checkbox_measure,
    .draw = checkbox_draw,
    .event = checkbox_event,
    .destroy = checkbox_destroy,
};

struct pg_widget *pg_checkbox_new(const char *label, bool active)
{
    struct checkbox *k = (struct checkbox *)pg_widget_new(&checkbox_class);
    if (!k)
        return NULL;
    k->label = pg__strdup(label);
    k->active = active;
    k->base.focusable = true;
    k->base.halign = PG_ALIGN_START;
    return &k->base;
}

bool pg_checkbox_active(const struct pg_widget *w)
{
    return w && w->cls == &checkbox_class && ((const struct checkbox *)w)->active;
}

void pg_checkbox_set_active(struct pg_widget *w, bool active)
{
    if (!w || w->cls != &checkbox_class)
        return;
    ((struct checkbox *)w)->active = active;
    pg_widget_queue_draw(w);
}

struct radio
{
    struct pg_widget base;
    char *label;
    bool active;
    struct radio *head;
    struct radio *next;
};

static void radio_measure(struct pg_widget *w, int *width, int *height)
{
    struct radio *r = (struct radio *)w;
    *width = 20 + (r->label && r->label[0] ? 10 + pgc_text_w(PGF_BODY, r->label) : 0);
    *height = 24;
}

static void radio_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct radio *r = (struct radio *)w;
    const struct pg_theme *p = pg_theme();
    int x = w->rect.x, y = w->rect.y + (w->rect.h - 20) / 2;
    bool hot = w->hovered && pg_widget_is_enabled(w);
    if (r->active)
    {
        pgc_circle(c, x + 10, y + 10, 10, disabled_mix(w, p->accent), 255);
        pgc_circle(c, x + 10, y + 10, 4, p->on_accent, 255);
    }
    else
    {
        pgc_circle(c, x + 10, y + 10, 10, disabled_mix(w, hot ? p->accent : p->border), 255);
        pgc_circle(c, x + 10, y + 10, 8, hot ? p->surface2 : (p->light ? 0xffffff : p->bg), 255);
    }
    if (r->label && r->label[0])
        pgc_text_fit(c, PGF_BODY, x + 30, y + (20 - pgc_font_h(PGF_BODY)) / 2, w->rect.w - 30, r->label,
                     disabled_mix(w, p->text));
    if (pg_widget_focus_visible(w))
        pgc_circle(c, x + 10, y + 10, 13, p->accent, 60);
}

static void radio_select(struct radio *r, bool emit)
{
    if (r->active)
        return;
    for (struct radio *q = r->head; q; q = q->next)
    {
        if (q->active)
        {
            q->active = false;
            pg_widget_queue_draw(&q->base);
        }
    }
    r->active = true;
    pg_widget_queue_draw(&r->base);
    if (emit)
        pg_widget_emit(&r->base, PG_SIGNAL_CHANGED);
}

static bool radio_event(struct pg_widget *w, const struct pg_event *e)
{
    struct radio *r = (struct radio *)w;
    int k = click_event(w, e);
    if (k == 2)
        radio_select(r, true);
    if (!k && e->type == PG_EVENT_KEY_DOWN && (e->key == PG_KEY_DOWN || e->key == PG_KEY_UP))
    {
        struct radio *target = NULL;
        if (e->key == PG_KEY_DOWN)
            target = r->next ? r->next : r->head;
        else
            for (struct radio *q = r->head; q; q = q->next)
                if (q->next == r || (r == r->head && !q->next))
                    target = q;
        if (target && target != r)
        {
            radio_select(target, true);
            pg_widget_grab_focus(&target->base);
        }
        return true;
    }
    return k != 0;
}

static void radio_destroy(struct pg_widget *w)
{
    struct radio *r = (struct radio *)w;
    free(r->label);
    struct radio *head = r->head;
    if (head == r)
    {
        struct radio *nh = r->next;
        for (struct radio *q = nh; q; q = q->next)
            q->head = nh;
    }
    else
    {
        for (struct radio *q = head; q; q = q->next)
            if (q->next == r)
            {
                q->next = r->next;
                break;
            }
    }
}

static const struct pg_widget_class radio_class = {
    .name = "radio",
    .size = sizeof(struct radio),
    .measure = radio_measure,
    .draw = radio_draw,
    .event = radio_event,
    .destroy = radio_destroy,
};

struct pg_widget *pg_radio_new(const char *label, struct pg_widget *group)
{
    struct radio *r = (struct radio *)pg_widget_new(&radio_class);
    if (!r)
        return NULL;
    r->label = pg__strdup(label);
    r->base.focusable = true;
    r->base.halign = PG_ALIGN_START;
    if (group && group->cls == &radio_class)
    {
        struct radio *g = (struct radio *)group;
        r->head = g->head;
        struct radio *q = r->head;
        while (q->next)
            q = q->next;
        q->next = r;
    }
    else
    {
        r->head = r;
        r->active = true;
    }
    return &r->base;
}

bool pg_radio_active(const struct pg_widget *w)
{
    return w && w->cls == &radio_class && ((const struct radio *)w)->active;
}

void pg_radio_set_active(struct pg_widget *w)
{
    if (w && w->cls == &radio_class)
        radio_select((struct radio *)w, false);
}

int pg_radio_group_index(const struct pg_widget *w)
{
    if (!w || w->cls != &radio_class)
        return -1;
    int i = 0;
    for (const struct radio *q = ((const struct radio *)w)->head; q; q = q->next, i++)
        if (q->active)
            return i;
    return -1;
}

struct slider
{
    struct pg_widget base;
    int min, max, value;
    uint32_t color;
    bool dragging;
};

#define SLIDER_H 28

static void slider_measure(struct pg_widget *w, int *width, int *height)
{
    (void)w;
    *width = 160;
    *height = SLIDER_H;
}

static int slider_track_x(const struct pg_widget *w)
{
    return w->rect.x + 10;
}

static int slider_track_w(const struct pg_widget *w)
{
    return pg__max(1, w->rect.w - 20);
}

static void slider_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct slider *s = (struct slider *)w;
    const struct pg_theme *p = pg_theme();
    int x = slider_track_x(w), tw = slider_track_w(w);
    int y = w->rect.y + (w->rect.h - SLIDER_H) / 2;
    const int th = 6;
    uint32_t color = disabled_mix(w, pg_color(s->color));
    int span = s->max - s->min ? s->max - s->min : 1;
    int fill = (int)((int64_t)(s->value - s->min) * tw / span);
    int ty = y + (SLIDER_H - th) / 2;
    pgc_rrect(c, x, ty, tw, th, th / 2, p->surface3, 255);
    if (fill > 0)
        pgc_rrect_grad(c, x, ty, fill > th ? fill : th, th, th / 2, pgc_mix(color, p->surface, 60), color, true);
    bool hot = (w->hovered || s->dragging) && pg_widget_is_enabled(w);
    int kr = hot ? 10 : 9;
    pgc_circle(c, x + fill, y + SLIDER_H / 2 + 1, kr + 1, 0x000000, 50);
    pgc_circle(c, x + fill, y + SLIDER_H / 2, kr, 0xffffff, 255);
    pgc_circle(c, x + fill, y + SLIDER_H / 2, kr - 5, color, 255);
    if (pg_widget_focus_visible(w))
        pgc_circle(c, x + fill, y + SLIDER_H / 2, kr + 4, p->accent, 70);
}

static void slider_apply(struct slider *s, int v, bool emit)
{
    v = pg__clamp(v, pg__min(s->min, s->max), pg__max(s->min, s->max));
    if (v == s->value)
        return;
    s->value = v;
    pg_widget_queue_draw(&s->base);
    if (emit)
        pg_widget_emit(&s->base, PG_SIGNAL_CHANGED);
}

static void slider_from_x(struct slider *s, int px)
{
    int x = slider_track_x(&s->base), tw = slider_track_w(&s->base);
    int v = s->min + (int)(((int64_t)(px - x) * (s->max - s->min) + tw / 2) / tw);
    slider_apply(s, v, true);
}

static bool slider_event(struct pg_widget *w, const struct pg_event *e)
{
    struct slider *s = (struct slider *)w;
    int step = pg__max(1, (s->max - s->min) / 20);
    switch (e->type)
    {
    case PG_EVENT_POINTER_DOWN:
        if (e->button != PG_BUTTON_LEFT)
            return false;
        s->dragging = true;
        slider_from_x(s, e->x);
        return true;
    case PG_EVENT_POINTER_MOVE:
        if (s->dragging)
            slider_from_x(s, e->x);
        return s->dragging;
    case PG_EVENT_POINTER_UP:
        if (!s->dragging)
            return false;
        s->dragging = false;
        pg_widget_queue_draw(w);
        pg_widget_emit(w, PG_SIGNAL_ACTIVATE);
        return true;
    case PG_EVENT_KEY_DOWN:
        if (e->key == PG_KEY_LEFT || e->key == PG_KEY_DOWN)
            slider_apply(s, s->value - step, true);
        else if (e->key == PG_KEY_RIGHT || e->key == PG_KEY_UP)
            slider_apply(s, s->value + step, true);
        else if (e->key == PG_KEY_HOME)
            slider_apply(s, s->min, true);
        else if (e->key == PG_KEY_END)
            slider_apply(s, s->max, true);
        else
            return false;
        pg_widget_emit(w, PG_SIGNAL_ACTIVATE);
        return true;
    case PG_EVENT_WHEEL:
        slider_apply(s, s->value + (e->wheel > 0 ? step : -step), true);
        pg_widget_emit(w, PG_SIGNAL_ACTIVATE);
        return true;
    default:
        return false;
    }
}

static const struct pg_widget_class slider_class = {
    .name = "slider",
    .size = sizeof(struct slider),
    .measure = slider_measure,
    .draw = slider_draw,
    .event = slider_event,
};

struct pg_widget *pg_slider_new(int min, int max, int value)
{
    struct slider *s = (struct slider *)pg_widget_new(&slider_class);
    if (!s)
        return NULL;
    s->min = min;
    s->max = max > min ? max : min + 1;
    s->value = pg__clamp(value, s->min, s->max);
    s->color = PG_COLOR_ACCENT;
    s->base.focusable = true;
    return &s->base;
}

int pg_slider_value(const struct pg_widget *w)
{
    return w && w->cls == &slider_class ? ((const struct slider *)w)->value : 0;
}

void pg_slider_set_value(struct pg_widget *w, int value)
{
    if (w && w->cls == &slider_class && !((struct slider *)w)->dragging)
        slider_apply((struct slider *)w, value, false);
}

void pg_slider_set_range(struct pg_widget *w, int min, int max)
{
    if (!w || w->cls != &slider_class)
        return;
    struct slider *s = (struct slider *)w;
    s->min = min;
    s->max = max > min ? max : min + 1;
    s->value = pg__clamp(s->value, s->min, s->max);
    pg_widget_queue_draw(w);
}

void pg_slider_set_color(struct pg_widget *w, uint32_t color)
{
    if (!w || w->cls != &slider_class)
        return;
    ((struct slider *)w)->color = color;
    pg_widget_queue_draw(w);
}

bool pg_slider_dragging(const struct pg_widget *w)
{
    return w && w->cls == &slider_class && ((const struct slider *)w)->dragging;
}

struct entry
{
    struct pg_widget base;
    char *buf;
    int len, cap;
    int cursor, anchor;
    bool password;
    char *placeholder;
    int scroll;
    uint32_t timer;
    bool caret;
    bool selecting;
};

#define ENTRY_PAD 12

static void entry_measure(struct pg_widget *w, int *width, int *height)
{
    (void)w;
    *width = 200;
    *height = 38;
}

static int utf8_prev(const char *s, int i)
{
    if (i <= 0)
        return 0;
    i--;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80)
        i--;
    return i;
}

static int utf8_next_i(const char *s, int len, int i)
{
    if (i >= len)
        return len;
    i++;
    while (i < len && ((unsigned char)s[i] & 0xC0) == 0x80)
        i++;
    return i;
}

static int shown_width(const struct entry *e, int bytes)
{
    if (e->password)
    {
        int n = 0;
        for (int i = 0; i < bytes; i = utf8_next_i(e->buf, e->len, i))
            n++;
        return n * pgc_text_w(PGF_BODY, "*");
    }
    char tmp[1024];
    int n = pg__min(bytes, (int)sizeof(tmp) - 1);
    memcpy(tmp, e->buf, (size_t)n);
    tmp[n] = 0;
    return pgc_text_w(PGF_BODY, tmp);
}

static void entry_fit(struct entry *e)
{
    int iw = pg__max(1, e->base.rect.w - 2 * ENTRY_PAD);
    int cx = shown_width(e, e->cursor);
    if (cx - e->scroll > iw)
        e->scroll = cx - iw;
    if (cx < e->scroll)
        e->scroll = cx;
    int total = shown_width(e, e->len);
    if (total - e->scroll < iw)
        e->scroll = pg__max(0, total - iw);
}

static void entry_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct entry *e = (struct entry *)w;
    const struct pg_theme *p = pg_theme();
    struct pg_rect r = w->rect;
    bool focus = pg_widget_has_focus(w);
    pgc_rrect(c, r.x, r.y, r.w, r.h, 8, p->light ? 0xffffff : p->bg, 255);
    pgc_rrect_stroke(c, r.x, r.y, r.w, r.h, 8, disabled_mix(w, focus ? p->accent : (w->hovered ? p->text3 : p->border)), 255);
    if (focus)
        pgc_rrect_stroke(c, r.x - 2, r.y - 2, r.w + 4, r.h + 4, 10, p->accent, 70);
    entry_fit(e);
    int ty = r.y + (r.h - pgc_font_h(PGF_BODY)) / 2;
    int x0 = r.x + ENTRY_PAD - e->scroll;
    struct pg_clip saved = pgc_clip_push(c, r.x + ENTRY_PAD - 2, r.y + 2, r.w - 2 * ENTRY_PAD + 4, r.h - 4);
    if (e->cursor != e->anchor && focus)
    {
        int a = pg__min(e->cursor, e->anchor), b = pg__max(e->cursor, e->anchor);
        int sx = x0 + shown_width(e, a), ex = x0 + shown_width(e, b);
        pgc_rrect(c, sx, r.y + 8, ex - sx, r.h - 16, 3, p->accent, 90);
    }
    if (e->len)
    {
        if (e->password)
        {
            int sw = pgc_text_w(PGF_BODY, "*"), x = x0;
            for (int i = 0; i < e->len; i = utf8_next_i(e->buf, e->len, i), x += sw)
                pgc_text(c, PGF_BODY, x, ty, "*", disabled_mix(w, p->text));
        }
        else
        {
            pgc_text(c, PGF_BODY, x0, ty, e->buf, disabled_mix(w, p->text));
        }
    }
    else if (e->placeholder && e->placeholder[0])
    {
        pgc_text(c, PGF_BODY, r.x + ENTRY_PAD, ty, e->placeholder, p->text3);
    }
    if (focus && e->caret)
        pgc_fill(c, x0 + shown_width(e, e->cursor), r.y + 9, 2, r.h - 18, p->accent);
    pgc_clip_pop(c, saved);
}

static bool entry_blink(void *data)
{
    struct entry *e = data;
    e->caret = !e->caret;
    pg_widget_queue_draw(&e->base);
    return true;
}

static void caret_reset(struct entry *e)
{
    e->caret = true;
    if (e->timer)
        pg_timer_remove(e->timer);
    e->timer = pg_widget_has_focus(&e->base) || e->base.focused ? pg_timer_add(530, entry_blink, e) : 0;
    pg_widget_queue_draw(&e->base);
}

static void delete_range(struct entry *e, int a, int b)
{
    if (a > b)
    {
        int t = a;
        a = b;
        b = t;
    }
    if (a == b)
        return;
    memmove(e->buf + a, e->buf + b, (size_t)(e->len - b + 1));
    e->len -= b - a;
    e->cursor = e->anchor = a;
}

static bool delete_selection(struct entry *e)
{
    if (e->cursor == e->anchor)
        return false;
    delete_range(e, e->cursor, e->anchor);
    return true;
}

static void insert_text(struct entry *e, const char *s, int n)
{
    delete_selection(e);
    if (e->len + n > e->cap)
        n = e->cap - e->len;
    if (n <= 0)
        return;
    memmove(e->buf + e->cursor + n, e->buf + e->cursor, (size_t)(e->len - e->cursor + 1));
    memcpy(e->buf + e->cursor, s, (size_t)n);
    e->len += n;
    e->cursor += n;
    e->anchor = e->cursor;
}

static int index_at(struct entry *e, int px)
{
    int target = px - (e->base.rect.x + ENTRY_PAD) + e->scroll;
    int best = 0, prev_w = 0;
    for (int i = 0; i <= e->len;)
    {
        int wv = shown_width(e, i);
        if (wv >= target)
        {
            best = (target - prev_w < wv - target && i > 0) ? utf8_prev(e->buf, i) : i;
            return best;
        }
        prev_w = wv;
        if (i == e->len)
            break;
        i = utf8_next_i(e->buf, e->len, i);
    }
    return e->len;
}

static bool entry_event(struct pg_widget *w, const struct pg_event *ev)
{
    struct entry *e = (struct entry *)w;
    bool shift = ev->mods & PG_MOD_SHIFT, ctrl = ev->mods & PG_MOD_CTRL;
    switch (ev->type)
    {
    case PG_EVENT_FOCUS_IN:
    case PG_EVENT_FOCUS_OUT:
        caret_reset(e);
        if (ev->type == PG_EVENT_FOCUS_OUT && e->timer)
        {
            pg_timer_remove(e->timer);
            e->timer = 0;
        }
        return false;
    case PG_EVENT_POINTER_DOWN:
        if (ev->button != PG_BUTTON_LEFT)
            return false;
        if (ev->clicks >= 2)
        {
            e->anchor = 0;
            e->cursor = e->len;
        }
        else
        {
            e->cursor = index_at(e, ev->x);
            if (!shift)
                e->anchor = e->cursor;
            e->selecting = true;
        }
        caret_reset(e);
        return true;
    case PG_EVENT_POINTER_MOVE:
        if (!e->selecting)
            return false;
        e->cursor = index_at(e, ev->x);
        pg_widget_queue_draw(w);
        return true;
    case PG_EVENT_POINTER_UP:
        e->selecting = false;
        return true;
    case PG_EVENT_KEY_DOWN:
        break;
    default:
        return false;
    }
    bool changed = false;
    int k = ev->key;
    if (ctrl && k == PG_KEY_A)
    {
        e->anchor = 0;
        e->cursor = e->len;
    }
    else if (k == PG_KEY_BACKSPACE)
    {
        if (!delete_selection(e) && e->cursor > 0)
            delete_range(e, utf8_prev(e->buf, e->cursor), e->cursor);
        changed = true;
    }
    else if (k == PG_KEY_DELETE)
    {
        if (!delete_selection(e) && e->cursor < e->len)
            delete_range(e, e->cursor, utf8_next_i(e->buf, e->len, e->cursor));
        changed = true;
    }
    else if (k == PG_KEY_LEFT || k == PG_KEY_RIGHT || k == PG_KEY_HOME || k == PG_KEY_END)
    {
        if (!shift && e->cursor != e->anchor && (k == PG_KEY_LEFT || k == PG_KEY_RIGHT))
        {
            e->cursor = k == PG_KEY_LEFT ? pg__min(e->cursor, e->anchor) : pg__max(e->cursor, e->anchor);
        }
        else if (k == PG_KEY_LEFT)
        {
            e->cursor = utf8_prev(e->buf, e->cursor);
        }
        else if (k == PG_KEY_RIGHT)
        {
            e->cursor = utf8_next_i(e->buf, e->len, e->cursor);
        }
        else
        {
            e->cursor = k == PG_KEY_HOME ? 0 : e->len;
        }
        if (!shift)
            e->anchor = e->cursor;
    }
    else if (k == PG_KEY_ENTER || k == PG_KEY_KP_ENTER)
    {
        pg_widget_emit(w, PG_SIGNAL_ACTIVATE);
        return true;
    }
    else if (ev->ch)
    {
        char ch = (char)ev->ch;
        insert_text(e, &ch, 1);
        changed = true;
    }
    else
    {
        return false;
    }
    caret_reset(e);
    if (changed)
        pg_widget_emit(w, PG_SIGNAL_CHANGED);
    return true;
}

static void entry_destroy(struct pg_widget *w)
{
    struct entry *e = (struct entry *)w;
    if (e->timer)
        pg_timer_remove(e->timer);
    free(e->buf);
    free(e->placeholder);
}

static const struct pg_widget_class entry_class = {
    .name = "entry",
    .size = sizeof(struct entry),
    .measure = entry_measure,
    .draw = entry_draw,
    .event = entry_event,
    .destroy = entry_destroy,
};

struct pg_widget *pg_entry_new(const char *placeholder)
{
    struct entry *e = (struct entry *)pg_widget_new(&entry_class);
    if (!e)
        return NULL;
    e->cap = 1023;
    e->buf = calloc(1, (size_t)e->cap + 1);
    e->placeholder = pg__strdup(placeholder);
    e->base.focusable = true;
    if (!e->buf)
    {
        pg_widget_destroy(&e->base);
        return NULL;
    }
    return &e->base;
}

static struct entry *as_entry(const struct pg_widget *w)
{
    return w && w->cls == &entry_class ? (struct entry *)w : NULL;
}

const char *pg_entry_text(const struct pg_widget *w)
{
    struct entry *e = as_entry(w);
    return e ? e->buf : "";
}

void pg_entry_set_text(struct pg_widget *w, const char *text)
{
    struct entry *e = as_entry(w);
    if (!e)
        return;
    int n = text ? (int)strlen(text) : 0;
    n = pg__min(n, e->cap);
    memcpy(e->buf, text ? text : "", (size_t)n);
    e->buf[n] = 0;
    e->len = n;
    e->cursor = e->anchor = n;
    pg_widget_queue_draw(w);
}

void pg_entry_set_password(struct pg_widget *w, bool password)
{
    struct entry *e = as_entry(w);
    if (!e)
        return;
    e->password = password;
    pg_widget_queue_draw(w);
}

void pg_entry_set_max_length(struct pg_widget *w, int max)
{
    struct entry *e = as_entry(w);
    if (!e || max <= 0)
        return;
    char *nb = realloc(e->buf, (size_t)max + 1);
    if (!nb)
        return;
    e->buf = nb;
    e->cap = max;
    if (e->len > max)
    {
        e->len = max;
        e->buf[max] = 0;
    }
    e->cursor = pg__min(e->cursor, e->len);
    e->anchor = pg__min(e->anchor, e->len);
}

void pg_entry_set_placeholder(struct pg_widget *w, const char *placeholder)
{
    struct entry *e = as_entry(w);
    if (!e)
        return;
    pg__set_text(&e->placeholder, placeholder);
    pg_widget_queue_draw(w);
}

void pg_entry_select_all(struct pg_widget *w)
{
    struct entry *e = as_entry(w);
    if (!e)
        return;
    e->anchor = 0;
    e->cursor = e->len;
    pg_widget_queue_draw(w);
}
