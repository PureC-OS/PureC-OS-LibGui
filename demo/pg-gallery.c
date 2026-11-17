#include <stdio.h>
#include <string.h>
#include "pg/pg.h"

struct gallery
{
    struct pg_window *win;
    struct pg_widget *status;
    struct pg_widget *progress;
    struct pg_widget *ring;
    struct pg_widget *slider_label;
    struct pg_widget *entry_echo;
    struct pg_widget *theme_dd;
    struct pg_menu *menu;
    int clicks;
    int canvas_x, canvas_y;
};

static struct gallery g;

static void set_status(const char *text)
{
    pg_label_set_text(g.status, text);
}

static void on_button(struct pg_widget *w, void *data)
{
    (void)w;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %d", (const char *)data, ++g.clicks);
    set_status(buf);
}

static void on_slider(struct pg_widget *w, void *data)
{
    (void)data;
    char buf[32];
    int v = pg_slider_value(w);
    snprintf(buf, sizeof(buf), "%d%%", v);
    pg_label_set_text(g.slider_label, buf);
    pg_progress_set(g.progress, (uint32_t)v * 10);
    pg_ring_set(g.ring, (uint32_t)v * 10);
    pg_ring_set_text(g.ring, buf);
}

static void on_toggle(struct pg_widget *w, void *data)
{
    set_status(pg_toggle_active(w) ? (const char *)data : "off");
}

static void on_entry(struct pg_widget *w, void *data)
{
    (void)data;
    pg_label_set_text(g.entry_echo, pg_entry_text(w));
}

static void on_entry_enter(struct pg_widget *w, void *data)
{
    (void)data;
    char buf[96];
    snprintf(buf, sizeof(buf), "Enter: %s", pg_entry_text(w));
    set_status(buf);
}

static void on_theme(struct pg_widget *w, void *data)
{
    (void)data;
    pg_theme_set(pg_dropdown_text(w));
    set_status(pg_dropdown_text(w));
}

static void on_menu(int index, void *data)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "menu: %s (%d)", (const char *)data, index);
    set_status(buf);
}

static void draw_canvas(struct pg_widget *w, struct pg_canvas *c, void *data)
{
    (void)data;
    const struct pg_theme *t = pg_theme();
    struct pg_rect r = w->rect;
    pgc_rrect(c, r.x, r.y, r.w, r.h, 12, t->surface2, 255);
    for (int i = 0; i < 6; i++)
    {
        float a = (float)i * 1.0471976f;
        float cx = (float)(r.x + r.w / 2), cy = (float)(r.y + r.h / 2);
        pgc_line(c, cx, cy, cx + pgc_cos(a) * 60.0f, cy + pgc_sin(a) * 60.0f, 2.0f, t->accent);
    }
    pgc_circle(c, g.canvas_x ? g.canvas_x : r.x + r.w / 2, g.canvas_y ? g.canvas_y : r.y + r.h / 2, 10,
               t->danger, 220);
    pgc_text(c, PGF_SMALL, r.x + 12, r.y + 10, "Right click: menu", t->text3);
}

static bool canvas_input(struct pg_widget *w, const struct pg_event *e, void *data)
{
    (void)data;
    if (e->type == PG_EVENT_POINTER_DOWN && e->button == PG_BUTTON_RIGHT)
    {
        pg_menu_popup(g.menu, w->window, e->x, e->y);
        return true;
    }
    if (e->type == PG_EVENT_POINTER_DOWN || (e->type == PG_EVENT_POINTER_MOVE && (e->buttons & PG_BUTTON_LEFT)))
    {
        g.canvas_x = e->x;
        g.canvas_y = e->y;
        pg_widget_queue_draw(w);
        return true;
    }
    return e->type == PG_EVENT_POINTER_UP;
}

static struct pg_widget *page(const char *title)
{
    struct pg_widget *box = pg_vbox(16);
    pg_box_set_padding(box, 24, 20, 24, 24);
    pg_widget_add(box, pg_label_new_font(title, PGF_TITLE, PG_COLOR_TEXT));
    return box;
}

static struct pg_widget *scrolled(struct pg_widget *content)
{
    struct pg_widget *s = pg_scroll_new();
    pg_widget_add(s, content);
    return s;
}

static struct pg_widget *row(const char *label, struct pg_widget *control)
{
    struct pg_widget *h = pg_hbox(12);
    struct pg_widget *l = pg_label_new(label);
    pg_widget_set_expand(l, true, false);
    pg_widget_set_align(l, PG_ALIGN_FILL, PG_ALIGN_CENTER);
    pg_widget_add(h, l);
    pg_widget_set_align(control, PG_ALIGN_END, PG_ALIGN_CENTER);
    pg_widget_add(h, control);
    return h;
}

static struct pg_widget *page_controls(void)
{
    struct pg_widget *p = page("Controls");
    struct pg_widget *buttons = pg_card_new("Buttons", "Four styles, icons, disabled state", PGI_CHECK_CIRCLE);
    struct pg_widget *bh = pg_hbox(10);
    struct pg_widget *b1 = pg_button_new("Primary", PG_NO_ICON, PG_BUTTON_PRIMARY);
    struct pg_widget *b2 = pg_button_new("Secondary", PGI_REFRESH, PG_BUTTON_SECONDARY);
    struct pg_widget *b3 = pg_button_new("Delete", PGI_CLOSE, PG_BUTTON_DANGER);
    struct pg_widget *b4 = pg_button_new("Ghost", PG_NO_ICON, PG_BUTTON_GHOST);
    struct pg_widget *b5 = pg_button_new("Disabled", PG_NO_ICON, PG_BUTTON_SECONDARY);
    pg_widget_set_enabled(b5, false);
    pg_widget_on(b1, PG_SIGNAL_CLICKED, on_button, "Primary");
    pg_widget_on(b2, PG_SIGNAL_CLICKED, on_button, "Secondary");
    pg_widget_on(b3, PG_SIGNAL_CLICKED, on_button, "Delete");
    pg_widget_on(b4, PG_SIGNAL_CLICKED, on_button, "Ghost");
    pg_widget_add(bh, b1);
    pg_widget_add(bh, b2);
    pg_widget_add(bh, b3);
    pg_widget_add(bh, b4);
    pg_widget_add(bh, b5);
    pg_widget_add(buttons, bh);
    pg_widget_add(p, buttons);

    struct pg_widget *sw = pg_card_new("Switches", NULL, PGI_COG);
    struct pg_widget *t1 = pg_toggle_new(true);
    pg_widget_on(t1, PG_SIGNAL_CHANGED, on_toggle, "Wi-Fi on");
    pg_widget_add(sw, row("Wi-Fi", t1));
    struct pg_widget *t2 = pg_toggle_new(false);
    pg_widget_on(t2, PG_SIGNAL_CHANGED, on_toggle, "Bluetooth on");
    pg_widget_add(sw, row("Bluetooth", t2));
    pg_widget_add(sw, pg_separator_new());
    pg_widget_add(sw, pg_checkbox_new("Show hidden files", true));
    pg_widget_add(sw, pg_checkbox_new("Sync clock with NTP", false));
    pg_widget_add(sw, pg_separator_new());
    struct pg_widget *r1 = pg_radio_new("Small", NULL);
    pg_widget_add(sw, r1);
    pg_widget_add(sw, pg_radio_new("Medium", r1));
    pg_widget_add(sw, pg_radio_new("Large", r1));
    pg_widget_add(p, sw);

    struct pg_widget *sl = pg_card_new("Sliders and progress", NULL, PGI_VOLUME_HIGH);
    struct pg_widget *grid = pg_grid_new(16, 12);
    struct pg_widget *slider = pg_slider_new(0, 100, 40);
    pg_widget_set_expand(slider, true, false);
    pg_widget_on(slider, PG_SIGNAL_CHANGED, on_slider, NULL);
    g.slider_label = pg_label_new_font("40%", PGF_MEDIUM, PG_COLOR_TEXT2);
    pg_widget_set_size(g.slider_label, 48, -1);
    pg_widget_set_align(g.slider_label, PG_ALIGN_FILL, PG_ALIGN_CENTER);
    pg_grid_attach(grid, pg_icon_new(PGI_VOLUME_LOW, PG_ICON_S, PG_COLOR_TEXT2), 0, 0, 1, 1);
    pg_grid_attach(grid, slider, 1, 0, 1, 1);
    pg_grid_attach(grid, g.slider_label, 2, 0, 1, 1);
    g.progress = pg_progress_new();
    pg_progress_set(g.progress, 400);
    pg_widget_set_align(g.progress, PG_ALIGN_FILL, PG_ALIGN_CENTER);
    pg_grid_attach(grid, g.progress, 1, 1, 1, 1);
    g.ring = pg_ring_new(36, 7);
    pg_ring_set(g.ring, 400);
    pg_ring_set_text(g.ring, "40%");
    pg_grid_attach(grid, g.ring, 3, 0, 1, 2);
    pg_widget_add(sl, grid);
    pg_widget_add(p, sl);
    return scrolled(p);
}

static const char *const sizes[] = {"Small", "Medium", "Large"};
static const char *const periods[] = {"Day", "Week", "Month", "Year"};

static struct pg_widget *page_inputs(void)
{
    struct pg_widget *p = page("Inputs");
    struct pg_widget *card = pg_card_new("Text", "Typing, selection, Home/End, password", PGI_KEYBOARD);
    struct pg_widget *entry = pg_entry_new("Type something...");
    pg_widget_on(entry, PG_SIGNAL_CHANGED, on_entry, NULL);
    pg_widget_on(entry, PG_SIGNAL_ACTIVATE, on_entry_enter, NULL);
    pg_widget_add(card, entry);
    g.entry_echo = pg_label_new_font("", PGF_SMALL, PG_COLOR_TEXT3);
    pg_widget_add(card, g.entry_echo);
    struct pg_widget *pass = pg_entry_new("Password");
    pg_entry_set_password(pass, true);
    pg_widget_add(card, pass);
    pg_widget_add(p, card);

    struct pg_widget *choice = pg_card_new("Choice", NULL, PGI_PALETTE);
    pg_widget_add(choice, row("Size", pg_dropdown_new(sizes, 3)));
    struct pg_widget *seg = pg_segmented_new(periods, 4);
    pg_widget_add(choice, seg);
    pg_widget_add(p, choice);

    struct pg_widget *tabs_card = pg_card_new("Tabs", NULL, PGI_FOLDER);
    static const char *const tab_names[] = {"General", "Network", "About"};
    struct pg_widget *tabs = pg_tabs_new(tab_names, 3);
    struct pg_widget *stack = pg_stack_new();
    pg_widget_add(stack, pg_kv_new("Host name", "purec"));
    pg_widget_add(stack, pg_kv_new("Address", "10.0.2.15"));
    struct pg_widget *about = pg_label_new("PureGUI 2: retained widgets, layout, CSD windows and popups on videod.");
    pg_label_set_wrap(about, true);
    pg_widget_add(stack, about);
    pg_tabs_bind_stack(tabs, stack);
    pg_widget_add(tabs_card, tabs);
    pg_widget_add(tabs_card, stack);
    pg_widget_add(p, tabs_card);

    struct pg_widget *draw = pg_card_new("Canvas", "Custom drawing with pg_drawing", PGI_WALLPAPER);
    pg_widget_add(draw, pg_drawing_new(200, 160, draw_canvas, canvas_input, NULL));
    pg_widget_add(p, draw);
    return scrolled(p);
}

static struct pg_widget *page_lists(void)
{
    struct pg_widget *p = page("Lists");
    struct pg_widget *card = pg_card_new("Networks", "Rows with icon, subtitle and trailing text", PGI_WIFI);
    struct pg_widget *list = pg_list_new();
    static const char *const names[] = {"Home", "Office", "Cafe", "Library", "Airport", "Guest"};
    for (int i = 0; i < 6; i++)
    {
        int idx = pg_list_add(list, PGI_WIFI, names[i], i % 2 ? "Secured, WPA2" : "Open");
        char buf[16];
        snprintf(buf, sizeof(buf), "-%d dBm", 45 + i * 8);
        pg_list_set_trailing(list, idx, buf);
    }
    pg_widget_add(card, list);
    pg_widget_add(p, card);

    struct pg_widget *info = pg_card_new("System", NULL, PGI_INFORMATION_OUTLINE);
    pg_widget_add(info, pg_kv_new("Kernel", "PureC OS 64-bit"));
    pg_widget_add(info, pg_kv_new("Memory", "512 MiB"));
    pg_widget_add(info, pg_kv_new("Display", "videod"));
    struct pg_widget *badges = pg_hbox(8);
    pg_widget_add(badges, pg_badge_new("online", PG_COLOR_SUCCESS));
    pg_widget_add(badges, pg_badge_new("update", PG_COLOR_WARNING));
    pg_widget_add(badges, pg_badge_new("error", PG_COLOR_DANGER));
    pg_widget_add(badges, pg_signal_new(-60));
    pg_widget_add(info, badges);
    pg_widget_add(p, info);
    return scrolled(p);
}

static struct pg_widget *page_theme(void)
{
    struct pg_widget *p = page("Appearance");
    struct pg_widget *card = pg_card_new("Theme", "Applied to every window at once", PGI_THEME_LIGHT_DARK);
    const char *names[32];
    unsigned n = pg_theme_count();
    int current = 0;
    for (unsigned i = 0; i < n && i < 32; i++)
    {
        names[i] = pg_theme_name(i);
        if (strcmp(names[i], pg_theme()->name) == 0)
            current = (int)i;
    }
    g.theme_dd = pg_dropdown_new(names, (int)n);
    pg_dropdown_set_selected(g.theme_dd, current);
    pg_widget_on(g.theme_dd, PG_SIGNAL_CHANGED, on_theme, NULL);
    pg_widget_add(card, row("Color scheme", g.theme_dd));
    struct pg_widget *swatches = pg_hbox(8);
    static const uint32_t roles[] = {PG_COLOR_ACCENT, PG_COLOR_SUCCESS, PG_COLOR_WARNING, PG_COLOR_DANGER, PG_COLOR_INFO};
    for (int i = 0; i < 5; i++)
        pg_widget_add(swatches, pg_icon_tile_new(PGI_PALETTE, 40, roles[i]));
    pg_widget_add(card, swatches);
    pg_widget_add(p, card);
    return scrolled(p);
}

static bool on_key(struct pg_window *win, const struct pg_event *e, void *data)
{
    (void)data;
    if ((e->mods & PG_MOD_CTRL) && e->key == PG_KEY_Q)
    {
        pg_window_close(win);
        return true;
    }
    return false;
}

struct pg_window *gallery_build(void)
{
    memset(&g, 0, sizeof(g));
    g.win = pg_window_new("PureGUI Gallery", 900, 620, PG_WINDOW_RESIZABLE);
    if (!g.win)
        return NULL;
    pg_window_set_min_size(g.win, 560, 380);
    pg_window_on_key(g.win, on_key, NULL);
    g.menu = pg_menu_new();
    pg_menu_add(g.menu, "Copy", PGI_FOLDER, on_menu, "copy");
    pg_menu_add(g.menu, "Refresh", PGI_REFRESH, on_menu, "refresh");
    pg_menu_add_separator(g.menu);
    pg_menu_add(g.menu, "Close", PGI_CLOSE, on_menu, "close");

    struct pg_widget *root = pg_hbox(0);
    struct pg_widget *side = pg_vbox(6);
    pg_box_set_padding(side, 12, 16, 12, 12);
    pg_box_set_background(side, PG_COLOR_SIDEBAR, 0);
    pg_widget_set_size(side, 220, -1);
    pg_widget_add(side, pg_label_new_font("PureGUI", PGF_HEAD, PG_COLOR_TEXT));
    struct pg_widget *nav = pg_list_new();
    pg_list_set_sidebar(nav, true);
    pg_list_add(nav, PGI_COG, "Controls", NULL);
    pg_list_add(nav, PGI_KEYBOARD, "Inputs", NULL);
    pg_list_add(nav, PGI_SERVER, "Lists", NULL);
    pg_list_add(nav, PGI_PALETTE, "Appearance", NULL);
    pg_widget_add(side, nav);
    pg_widget_add(side, pg_spacer_new());
    g.status = pg_label_new_font("Ready", PGF_SMALL, PG_COLOR_TEXT3);
    pg_label_set_ellipsize(g.status, true);
    pg_widget_add(side, g.status);
    pg_widget_add(root, side);

    struct pg_widget *stack = pg_stack_new();
    pg_widget_set_expand(stack, true, true);
    pg_widget_add(stack, page_controls());
    pg_widget_add(stack, page_inputs());
    pg_widget_add(stack, page_lists());
    pg_widget_add(stack, page_theme());
    pg_list_bind_stack(nav, stack);
    pg_widget_add(root, stack);
    pg_window_set_child(g.win, root);
    return g.win;
}

#ifndef PG_GALLERY_NO_MAIN
int main(int argc, char **argv)
{
    if (!pg_init(argc, argv))
    {
        fprintf(stderr, "pg-gallery: cannot connect to videod\n");
        return 1;
    }
    if (!gallery_build())
    {
        fprintf(stderr, "pg-gallery: cannot create window\n");
        return 1;
    }
    int code = pg_run();
    pg_menu_destroy(g.menu);
    pg_shutdown();
    return code;
}
#endif
