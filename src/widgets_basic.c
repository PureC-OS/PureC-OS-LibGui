#include "internal.h"

struct label
{
    struct pg_widget base;
    char *text;
    int font;
    uint32_t color;
    bool wrap;
    bool ellipsize;
    int align;
};

static int line_count_width(int font, const char *s, int *lines)
{
    int best = 0, n = 1;
    char buf[512];
    while (s && *s)
    {
        int k = 0;
        while (s[k] && s[k] != '\n' && k < (int)sizeof(buf) - 1)
        {
            buf[k] = s[k];
            k++;
        }
        buf[k] = 0;
        best = pg__max(best, pgc_text_w(font, buf));
        s += k;
        if (*s == '\n')
        {
            s++;
            n++;
        }
    }
    if (lines)
        *lines = n;
    return best;
}

static void label_measure(struct pg_widget *w, int *width, int *height)
{
    struct label *l = (struct label *)w;
    int lines = 1;
    int tw = line_count_width(l->font, l->text, &lines);
    if (l->wrap)
        tw = pg__min(tw, 420);
    if (l->ellipsize)
        tw = pg__min(tw, 40);
    *width = tw;
    *height = l->wrap ? pgc_text_wrap(NULL, l->font, 0, 0, pg__max(tw, 1), l->text, 0) - 4
                      : lines * (pgc_font_h(l->font) + 4) - 4;
    if (*height < pgc_font_h(l->font))
        *height = pgc_font_h(l->font);
}

static int label_hfw(struct pg_widget *w, int width)
{
    struct label *l = (struct label *)w;
    if (!l->wrap)
    {
        int mw, mh;
        label_measure(w, &mw, &mh);
        return mh;
    }
    return pg__max(pgc_font_h(l->font), pgc_text_wrap(NULL, l->font, 0, 0, pg__max(width, 1), l->text, 0) - 4);
}

static void label_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct label *l = (struct label *)w;
    uint32_t color = pg_color(l->color);
    if (!pg_widget_is_enabled(w))
        color = pgc_mix(color, pg_theme()->bg, 110);
    struct pg_rect r = w->rect;
    if (l->wrap)
    {
        pgc_text_wrap(c, l->font, r.x, r.y, r.w, l->text, color);
        return;
    }
    int lh = pgc_font_h(l->font) + 4;
    int lines = 1;
    line_count_width(l->font, l->text, &lines);
    int y = r.y + (r.h - (lines * lh - 4)) / 2;
    const char *s = l->text;
    char buf[512];
    while (s)
    {
        int k = 0;
        while (s[k] && s[k] != '\n' && k < (int)sizeof(buf) - 1)
        {
            buf[k] = s[k];
            k++;
        }
        buf[k] = 0;
        int tw = pgc_text_w(l->font, buf);
        int x = r.x;
        if (l->align == PG_ALIGN_CENTER)
            x = r.x + (r.w - tw) / 2;
        else if (l->align == PG_ALIGN_END)
            x = r.x + r.w - tw;
        if (tw > r.w)
            pgc_text_fit(c, l->font, r.x, y, r.w, buf, color);
        else
            pgc_text(c, l->font, x, y, buf, color);
        y += lh;
        s += k;
        if (*s != '\n')
            break;
        s++;
    }
}

static void label_destroy(struct pg_widget *w)
{
    free(((struct label *)w)->text);
}

static const struct pg_widget_class label_class = {
    .name = "label",
    .size = sizeof(struct label),
    .measure = label_measure,
    .height_for_width = label_hfw,
    .draw = label_draw,
    .destroy = label_destroy,
};

struct pg_widget *pg_label_new_font(const char *text, int font, uint32_t color)
{
    struct label *l = (struct label *)pg_widget_new(&label_class);
    if (!l)
        return NULL;
    l->text = pg__strdup(text);
    l->font = font;
    l->color = color;
    return &l->base;
}

struct pg_widget *pg_label_new(const char *text)
{
    return pg_label_new_font(text, PGF_BODY, PG_COLOR_TEXT);
}

static struct label *as_label(const struct pg_widget *w)
{
    return w && w->cls == &label_class ? (struct label *)w : NULL;
}

void pg_label_set_text(struct pg_widget *w, const char *text)
{
    struct label *l = as_label(w);
    if (!l || strcmp(l->text, text ? text : "") == 0)
        return;
    pg__set_text(&l->text, text);
    pg_widget_queue_layout(w);
    pg_widget_queue_draw(w);
}

const char *pg_label_text(const struct pg_widget *w)
{
    struct label *l = as_label(w);
    return l ? l->text : "";
}

void pg_label_set_font(struct pg_widget *w, int font)
{
    struct label *l = as_label(w);
    if (!l)
        return;
    l->font = font;
    pg_widget_queue_layout(w);
}

void pg_label_set_color(struct pg_widget *w, uint32_t color)
{
    struct label *l = as_label(w);
    if (!l)
        return;
    l->color = color;
    pg_widget_queue_draw(w);
}

void pg_label_set_wrap(struct pg_widget *w, bool wrap)
{
    struct label *l = as_label(w);
    if (!l)
        return;
    l->wrap = wrap;
    pg_widget_queue_layout(w);
}

void pg_label_set_ellipsize(struct pg_widget *w, bool ellipsize)
{
    struct label *l = as_label(w);
    if (!l)
        return;
    l->ellipsize = ellipsize;
    pg_widget_queue_layout(w);
}

void pg_label_set_text_align(struct pg_widget *w, enum pg_align align)
{
    struct label *l = as_label(w);
    if (!l)
        return;
    l->align = align;
    pg_widget_queue_draw(w);
}

struct icon
{
    struct pg_widget base;
    int icon;
    int size;
    uint32_t color;
    bool tile;
    int tile_size;
};

static void icon_measure(struct pg_widget *w, int *width, int *height)
{
    struct icon *i = (struct icon *)w;
    int s = i->tile ? i->tile_size : pgc_icon_px(i->size);
    *width = s;
    *height = s;
}

static void icon_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct icon *i = (struct icon *)w;
    uint32_t color = pg_color(i->color);
    if (i->tile)
    {
        int s = i->tile_size, px = pgc_icon_px(i->size);
        int x = w->rect.x + (w->rect.w - s) / 2, y = w->rect.y + (w->rect.h - s) / 2;
        pgc_rrect(c, x, y, s, s, s / 3, color, pg_theme()->light ? 40 : 46);
        pgc_icon(c, i->icon, i->size, x + (s - px) / 2, y + (s - px) / 2, color);
        return;
    }
    int px = pgc_icon_px(i->size);
    pgc_icon(c, i->icon, i->size, w->rect.x + (w->rect.w - px) / 2, w->rect.y + (w->rect.h - px) / 2, color);
}

static const struct pg_widget_class icon_class = {
    .name = "icon",
    .size = sizeof(struct icon),
    .measure = icon_measure,
    .draw = icon_draw,
};

struct pg_widget *pg_icon_new(int icon, int size, uint32_t color)
{
    struct icon *i = (struct icon *)pg_widget_new(&icon_class);
    if (!i)
        return NULL;
    i->icon = icon;
    i->size = pg__clamp(size, 0, PGF_ICON_SIZES - 1);
    i->color = color;
    return &i->base;
}

struct pg_widget *pg_icon_tile_new(int icon, int size, uint32_t color)
{
    struct pg_widget *w = pg_icon_new(icon, PG_ICON_S, color);
    if (!w)
        return NULL;
    struct icon *i = (struct icon *)w;
    i->tile = true;
    i->tile_size = size > 0 ? size : 34;
    if (i->tile_size >= 64)
        i->size = PG_ICON_M;
    return w;
}

void pg_icon_set(struct pg_widget *w, int icon, uint32_t color)
{
    if (!w || w->cls != &icon_class)
        return;
    struct icon *i = (struct icon *)w;
    i->icon = icon;
    i->color = color;
    pg_widget_queue_draw(w);
}

static void sep_measure(struct pg_widget *w, int *width, int *height)
{
    (void)w;
    *width = 1;
    *height = 1;
}

static void sep_draw(struct pg_widget *w, struct pg_canvas *c)
{
    const struct pg_theme *t = pg_theme();
    if (w->rect.w >= w->rect.h)
        pgc_fill_a(c, w->rect.x, w->rect.y + w->rect.h / 2, w->rect.w, 1, t->border, t->light ? 200 : 140);
    else
        pgc_fill_a(c, w->rect.x + w->rect.w / 2, w->rect.y, 1, w->rect.h, t->border, t->light ? 200 : 140);
}

static const struct pg_widget_class separator_class = {
    .name = "separator",
    .measure = sep_measure,
    .draw = sep_draw,
};

struct pg_widget *pg_separator_new(void)
{
    return pg_widget_new(&separator_class);
}

static const struct pg_widget_class spacer_class = {
    .name = "spacer",
};

struct pg_widget *pg_spacer_new(void)
{
    struct pg_widget *w = pg_widget_new(&spacer_class);
    if (w)
    {
        w->hexpand = true;
        w->vexpand = true;
    }
    return w;
}

struct badge
{
    struct pg_widget base;
    char *text;
    uint32_t color;
};

static void badge_measure(struct pg_widget *w, int *width, int *height)
{
    struct badge *b = (struct badge *)w;
    *width = pgc_text_w(PGF_SMALL, b->text) + 16;
    *height = 22;
}

static void badge_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct badge *b = (struct badge *)w;
    uint32_t color = pg_color(b->color);
    int y = w->rect.y + (w->rect.h - 22) / 2;
    pgc_rrect(c, w->rect.x, y, w->rect.w, 22, 11, color, pg_theme()->light ? 40 : 46);
    pgc_text_center(c, PGF_SMALL, w->rect.x, y + 3, w->rect.w, b->text, color);
}

static void badge_destroy(struct pg_widget *w)
{
    free(((struct badge *)w)->text);
}

static const struct pg_widget_class badge_class = {
    .name = "badge",
    .size = sizeof(struct badge),
    .measure = badge_measure,
    .draw = badge_draw,
    .destroy = badge_destroy,
};

struct pg_widget *pg_badge_new(const char *text, uint32_t color)
{
    struct badge *b = (struct badge *)pg_widget_new(&badge_class);
    if (!b)
        return NULL;
    b->text = pg__strdup(text);
    b->color = color;
    b->base.halign = PG_ALIGN_START;
    b->base.valign = PG_ALIGN_CENTER;
    return &b->base;
}

void pg_badge_set(struct pg_widget *w, const char *text, uint32_t color)
{
    if (!w || w->cls != &badge_class)
        return;
    struct badge *b = (struct badge *)w;
    pg__set_text(&b->text, text);
    b->color = color;
    pg_widget_queue_layout(w);
    pg_widget_queue_draw(w);
}

struct kv
{
    struct pg_widget base;
    char *key;
    char *value;
};

static void kv_measure(struct pg_widget *w, int *width, int *height)
{
    struct kv *k = (struct kv *)w;
    *width = pgc_text_w(PGF_BODY, k->key) + 16 + pgc_text_w(PGF_MEDIUM, k->value);
    *height = pgc_font_h(PGF_MEDIUM) + 6;
}

static void kv_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct kv *k = (struct kv *)w;
    const struct pg_theme *t = pg_theme();
    struct pg_rect r = w->rect;
    int y = r.y + (r.h - pgc_font_h(PGF_MEDIUM)) / 2;
    pgc_text(c, PGF_BODY, r.x, y, k->key, t->text3);
    int kw = pgc_text_w(PGF_BODY, k->key) + 16;
    int vw = pgc_text_w(PGF_MEDIUM, k->value);
    if (vw > r.w - kw)
        pgc_text_fit(c, PGF_MEDIUM, r.x + kw, y, r.w - kw, k->value, t->text);
    else
        pgc_text(c, PGF_MEDIUM, r.x + r.w - vw, y, k->value, t->text);
}

static void kv_destroy(struct pg_widget *w)
{
    struct kv *k = (struct kv *)w;
    free(k->key);
    free(k->value);
}

static const struct pg_widget_class kv_class = {
    .name = "kv",
    .size = sizeof(struct kv),
    .measure = kv_measure,
    .draw = kv_draw,
    .destroy = kv_destroy,
};

struct pg_widget *pg_kv_new(const char *key, const char *value)
{
    struct kv *k = (struct kv *)pg_widget_new(&kv_class);
    if (!k)
        return NULL;
    k->key = pg__strdup(key);
    k->value = pg__strdup(value);
    return &k->base;
}

void pg_kv_set_value(struct pg_widget *w, const char *value)
{
    if (!w || w->cls != &kv_class)
        return;
    struct kv *k = (struct kv *)w;
    if (strcmp(k->value, value ? value : "") == 0)
        return;
    pg__set_text(&k->value, value);
    pg_widget_queue_draw(w);
}

struct signal_bars
{
    struct pg_widget base;
    int rssi;
};

static void signal_measure(struct pg_widget *w, int *width, int *height)
{
    (void)w;
    *width = pgc_icon_px(PG_ICON_S);
    *height = pgc_icon_px(PG_ICON_S);
}

static void signal_draw(struct pg_widget *w, struct pg_canvas *c)
{
    int rssi = ((struct signal_bars *)w)->rssi;
    int level = rssi >= -55 ? 4 : rssi >= -65 ? 3
                              : rssi >= -75   ? 2
                              : rssi >= -85   ? 1
                                              : 0;
    static const int ids[5] = {PGI_WIFI_STRENGTH_OUTLINE, PGI_WIFI_STRENGTH_1, PGI_WIFI_STRENGTH_2,
                               PGI_WIFI_STRENGTH_3, PGI_WIFI_STRENGTH_4};
    int px = pgc_icon_px(PG_ICON_S);
    pgc_icon(c, ids[level], PG_ICON_S, w->rect.x + (w->rect.w - px) / 2, w->rect.y + (w->rect.h - px) / 2,
             level ? pg_theme()->text : pg_theme()->text3);
}

static const struct pg_widget_class signal_class = {
    .name = "signal",
    .size = sizeof(struct signal_bars),
    .measure = signal_measure,
    .draw = signal_draw,
};

struct pg_widget *pg_signal_new(int rssi)
{
    struct signal_bars *s = (struct signal_bars *)pg_widget_new(&signal_class);
    if (s)
        s->rssi = rssi;
    return s ? &s->base : NULL;
}

void pg_signal_set(struct pg_widget *w, int rssi)
{
    if (!w || w->cls != &signal_class)
        return;
    ((struct signal_bars *)w)->rssi = rssi;
    pg_widget_queue_draw(w);
}

struct progress
{
    struct pg_widget base;
    uint32_t permille;
    uint32_t color;
};

static void progress_measure(struct pg_widget *w, int *width, int *height)
{
    (void)w;
    *width = 120;
    *height = 8;
}

static void progress_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct progress *p = (struct progress *)w;
    const struct pg_theme *t = pg_theme();
    struct pg_rect r = w->rect;
    int h = pg__min(r.h, 8), y = r.y + (r.h - h) / 2;
    uint32_t color = pg_color(p->color);
    pgc_rrect(c, r.x, y, r.w, h, h / 2, t->surface3, 255);
    int fill = (int)((uint64_t)r.w * p->permille / 1000);
    if (fill > 0)
        pgc_rrect_grad(c, r.x, y, fill < h ? h : fill, h, h / 2, pgc_mix(color, t->surface, 50), color, true);
}

static const struct pg_widget_class progress_class = {
    .name = "progress",
    .size = sizeof(struct progress),
    .measure = progress_measure,
    .draw = progress_draw,
};

struct pg_widget *pg_progress_new(void)
{
    struct progress *p = (struct progress *)pg_widget_new(&progress_class);
    if (p)
        p->color = PG_COLOR_ACCENT;
    return p ? &p->base : NULL;
}

void pg_progress_set(struct pg_widget *w, uint32_t permille)
{
    if (!w || w->cls != &progress_class)
        return;
    struct progress *p = (struct progress *)w;
    permille = permille > 1000 ? 1000 : permille;
    if (p->permille == permille)
        return;
    p->permille = permille;
    pg_widget_queue_draw(w);
}

void pg_progress_set_color(struct pg_widget *w, uint32_t color)
{
    if (!w || w->cls != &progress_class)
        return;
    ((struct progress *)w)->color = color;
    pg_widget_queue_draw(w);
}

struct ring
{
    struct pg_widget base;
    int radius, thick;
    uint32_t permille;
    uint32_t color;
    char *text;
};

static void ring_measure(struct pg_widget *w, int *width, int *height)
{
    struct ring *r = (struct ring *)w;
    *width = r->radius * 2 + 2;
    *height = r->radius * 2 + 2;
}

static void ring_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct ring *r = (struct ring *)w;
    const struct pg_theme *t = pg_theme();
    int cx = w->rect.x + w->rect.w / 2, cy = w->rect.y + w->rect.h / 2;
    pgc_ring(c, cx, cy, r->radius, r->thick, r->permille, pg_color(r->color), t->surface3);
    if (r->text && r->text[0])
    {
        int font = r->radius >= 40 ? PGF_HEAD : PGF_MEDIUM;
        pgc_text_center(c, font, w->rect.x, cy - pgc_font_h(font) / 2, w->rect.w, r->text, t->text);
    }
}

static void ring_destroy(struct pg_widget *w)
{
    free(((struct ring *)w)->text);
}

static const struct pg_widget_class ring_class = {
    .name = "ring",
    .size = sizeof(struct ring),
    .measure = ring_measure,
    .draw = ring_draw,
    .destroy = ring_destroy,
};

struct pg_widget *pg_ring_new(int radius, int thickness)
{
    struct ring *r = (struct ring *)pg_widget_new(&ring_class);
    if (!r)
        return NULL;
    r->radius = radius > 4 ? radius : 32;
    r->thick = thickness > 0 ? thickness : 6;
    r->color = PG_COLOR_ACCENT;
    r->base.halign = PG_ALIGN_CENTER;
    r->base.valign = PG_ALIGN_CENTER;
    return &r->base;
}

void pg_ring_set(struct pg_widget *w, uint32_t permille)
{
    if (!w || w->cls != &ring_class)
        return;
    ((struct ring *)w)->permille = permille > 1000 ? 1000 : permille;
    pg_widget_queue_draw(w);
}

void pg_ring_set_color(struct pg_widget *w, uint32_t color)
{
    if (!w || w->cls != &ring_class)
        return;
    ((struct ring *)w)->color = color;
    pg_widget_queue_draw(w);
}

void pg_ring_set_text(struct pg_widget *w, const char *text)
{
    if (!w || w->cls != &ring_class)
        return;
    pg__set_text(&((struct ring *)w)->text, text);
    pg_widget_queue_draw(w);
}

struct image
{
    struct pg_widget base;
    uint32_t *px;
    int w, h;
    int radius;
};

static void image_measure(struct pg_widget *w, int *width, int *height)
{
    struct image *i = (struct image *)w;
    *width = i->w;
    *height = i->h;
}

static void image_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct image *i = (struct image *)w;
    if (!i->px)
        return;
    pgc_image(c, w->rect.x + (w->rect.w - i->w) / 2, w->rect.y + (w->rect.h - i->h) / 2, i->w, i->h, i->px,
              i->radius);
}

static void image_destroy(struct pg_widget *w)
{
    free(((struct image *)w)->px);
}

static const struct pg_widget_class image_class = {
    .name = "image",
    .size = sizeof(struct image),
    .measure = image_measure,
    .draw = image_draw,
    .destroy = image_destroy,
};

struct pg_widget *pg_image_new(const uint32_t *pixels, int width, int height, int radius)
{
    struct image *i = (struct image *)pg_widget_new(&image_class);
    if (!i)
        return NULL;
    i->radius = radius;
    pg_image_set(&i->base, pixels, width, height);
    return &i->base;
}

void pg_image_set(struct pg_widget *w, const uint32_t *pixels, int width, int height)
{
    if (!w || w->cls != &image_class)
        return;
    struct image *i = (struct image *)w;
    free(i->px);
    i->px = NULL;
    i->w = i->h = 0;
    if (pixels && width > 0 && height > 0)
    {
        i->px = malloc((size_t)width * (size_t)height * 4);
        if (i->px)
        {
            memcpy(i->px, pixels, (size_t)width * (size_t)height * 4);
            i->w = width;
            i->h = height;
        }
    }
    pg_widget_queue_layout(w);
    pg_widget_queue_draw(w);
}

struct drawing
{
    struct pg_widget base;
    int w, h;
    pg_draw_fn draw;
    pg_input_fn input;
    void *data;
};

static void drawing_measure(struct pg_widget *w, int *width, int *height)
{
    struct drawing *d = (struct drawing *)w;
    *width = d->w;
    *height = d->h;
}

static void drawing_draw(struct pg_widget *w, struct pg_canvas *c)
{
    struct drawing *d = (struct drawing *)w;
    if (!d->draw)
        return;
    struct pg_clip saved = pgc_clip_push(c, w->rect.x, w->rect.y, w->rect.w, w->rect.h);
    d->draw(w, c, d->data);
    pgc_clip_pop(c, saved);
}

static bool drawing_event(struct pg_widget *w, const struct pg_event *e)
{
    struct drawing *d = (struct drawing *)w;
    return d->input ? d->input(w, e, d->data) : false;
}

static const struct pg_widget_class drawing_class = {
    .name = "drawing",
    .size = sizeof(struct drawing),
    .measure = drawing_measure,
    .draw = drawing_draw,
    .event = drawing_event,
};

struct pg_widget *pg_drawing_new(int width, int height, pg_draw_fn draw, pg_input_fn input, void *data)
{
    struct drawing *d = (struct drawing *)pg_widget_new(&drawing_class);
    if (!d)
        return NULL;
    d->w = width;
    d->h = height;
    d->draw = draw;
    d->input = input;
    d->data = data;
    d->base.focusable = input != NULL;
    return &d->base;
}
