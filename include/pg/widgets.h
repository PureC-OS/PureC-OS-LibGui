#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "widget.h"
#include "window.h"

enum pg_button_style {
    PG_BUTTON_PRIMARY = 0,
    PG_BUTTON_SECONDARY,
    PG_BUTTON_DANGER,
    PG_BUTTON_GHOST,
    PG_BUTTON_FLAT,
};

#define PG_NO_ICON (-1)

struct pg_widget *pg_box_new(enum pg_orientation orientation, int spacing);
void pg_box_set_spacing(struct pg_widget *box, int spacing);
void pg_box_set_padding(struct pg_widget *box, int left, int top, int right, int bottom);
void pg_box_set_background(struct pg_widget *box, uint32_t color, int radius);
struct pg_widget *pg_hbox(int spacing);
struct pg_widget *pg_vbox(int spacing);

struct pg_widget *pg_grid_new(int column_spacing, int row_spacing);
void pg_grid_attach(struct pg_widget *grid, struct pg_widget *child, int col, int row, int col_span, int row_span);

struct pg_widget *pg_stack_new(void);
void pg_stack_set_index(struct pg_widget *stack, int index);
int pg_stack_index(const struct pg_widget *stack);

struct pg_widget *pg_scroll_new(void);
void pg_scroll_to(struct pg_widget *scroll, int y);
int pg_scroll_offset(const struct pg_widget *scroll);
void pg_scroll_reveal(struct pg_widget *scroll, const struct pg_widget *child);

struct pg_widget *pg_card_new(const char *title, const char *subtitle, int icon);
void pg_card_set_title(struct pg_widget *card, const char *title, const char *subtitle);
void pg_card_set_padding(struct pg_widget *card, int padding);
void pg_card_set_spacing(struct pg_widget *card, int spacing);

struct pg_widget *pg_label_new(const char *text);
struct pg_widget *pg_label_new_font(const char *text, int font, uint32_t color);
void pg_label_set_text(struct pg_widget *label, const char *text);
const char *pg_label_text(const struct pg_widget *label);
void pg_label_set_font(struct pg_widget *label, int font);
void pg_label_set_color(struct pg_widget *label, uint32_t color);
void pg_label_set_wrap(struct pg_widget *label, bool wrap);
void pg_label_set_ellipsize(struct pg_widget *label, bool ellipsize);
void pg_label_set_text_align(struct pg_widget *label, enum pg_align align);

struct pg_widget *pg_icon_new(int icon, int size, uint32_t color);
void pg_icon_set(struct pg_widget *w, int icon, uint32_t color);
struct pg_widget *pg_icon_tile_new(int icon, int size, uint32_t color);

struct pg_widget *pg_button_new(const char *label, int icon, enum pg_button_style style);
void pg_button_set_label(struct pg_widget *button, const char *label);
void pg_button_set_icon(struct pg_widget *button, int icon);
void pg_button_set_style(struct pg_widget *button, enum pg_button_style style);

struct pg_widget *pg_toggle_new(bool active);
bool pg_toggle_active(const struct pg_widget *toggle);
void pg_toggle_set_active(struct pg_widget *toggle, bool active);

struct pg_widget *pg_checkbox_new(const char *label, bool active);
bool pg_checkbox_active(const struct pg_widget *checkbox);
void pg_checkbox_set_active(struct pg_widget *checkbox, bool active);

struct pg_widget *pg_radio_new(const char *label, struct pg_widget *group);
bool pg_radio_active(const struct pg_widget *radio);
void pg_radio_set_active(struct pg_widget *radio);
int pg_radio_group_index(const struct pg_widget *radio);

struct pg_widget *pg_slider_new(int min, int max, int value);
int pg_slider_value(const struct pg_widget *slider);
void pg_slider_set_value(struct pg_widget *slider, int value);
void pg_slider_set_range(struct pg_widget *slider, int min, int max);
void pg_slider_set_color(struct pg_widget *slider, uint32_t color);
bool pg_slider_dragging(const struct pg_widget *slider);

struct pg_widget *pg_progress_new(void);
void pg_progress_set(struct pg_widget *progress, uint32_t permille);
void pg_progress_set_color(struct pg_widget *progress, uint32_t color);

struct pg_widget *pg_ring_new(int radius, int thickness);
void pg_ring_set(struct pg_widget *ring, uint32_t permille);
void pg_ring_set_color(struct pg_widget *ring, uint32_t color);
void pg_ring_set_text(struct pg_widget *ring, const char *text);

struct pg_widget *pg_entry_new(const char *placeholder);
const char *pg_entry_text(const struct pg_widget *entry);
void pg_entry_set_text(struct pg_widget *entry, const char *text);
void pg_entry_set_password(struct pg_widget *entry, bool password);
void pg_entry_set_max_length(struct pg_widget *entry, int max);
void pg_entry_set_placeholder(struct pg_widget *entry, const char *placeholder);
void pg_entry_select_all(struct pg_widget *entry);

struct pg_widget *pg_segmented_new(const char *const *labels, int count);
int pg_segmented_selected(const struct pg_widget *seg);
void pg_segmented_set_selected(struct pg_widget *seg, int index);

struct pg_widget *pg_tabs_new(const char *const *labels, int count);
int pg_tabs_selected(const struct pg_widget *tabs);
void pg_tabs_set_selected(struct pg_widget *tabs, int index);
void pg_tabs_bind_stack(struct pg_widget *tabs, struct pg_widget *stack);

struct pg_widget *pg_dropdown_new(const char *const *labels, int count);
int pg_dropdown_selected(const struct pg_widget *dropdown);
void pg_dropdown_set_selected(struct pg_widget *dropdown, int index);
void pg_dropdown_set_items(struct pg_widget *dropdown, const char *const *labels, int count);
const char *pg_dropdown_text(const struct pg_widget *dropdown);

struct pg_widget *pg_list_new(void);
int pg_list_add(struct pg_widget *list, int icon, const char *title, const char *subtitle);
void pg_list_clear(struct pg_widget *list);
int pg_list_count(const struct pg_widget *list);
int pg_list_selected(const struct pg_widget *list);
void pg_list_set_selected(struct pg_widget *list, int index);
void pg_list_set_item(struct pg_widget *list, int index, int icon, const char *title, const char *subtitle);
void pg_list_set_trailing(struct pg_widget *list, int index, const char *text);
void pg_list_set_sidebar(struct pg_widget *list, bool sidebar);
void pg_list_bind_stack(struct pg_widget *list, struct pg_widget *stack);
int pg_list_activated(const struct pg_widget *list);

struct pg_widget *pg_separator_new(void);
struct pg_widget *pg_spacer_new(void);

struct pg_widget *pg_badge_new(const char *text, uint32_t color);
void pg_badge_set(struct pg_widget *badge, const char *text, uint32_t color);

struct pg_widget *pg_kv_new(const char *key, const char *value);
void pg_kv_set_value(struct pg_widget *kv, const char *value);

struct pg_widget *pg_signal_new(int rssi);
void pg_signal_set(struct pg_widget *signal, int rssi);

struct pg_widget *pg_image_new(const uint32_t *pixels, int width, int height, int radius);
void pg_image_set(struct pg_widget *image, const uint32_t *pixels, int width, int height);

typedef void (*pg_draw_fn)(struct pg_widget *w, struct pg_canvas *c, void *data);
typedef bool (*pg_input_fn)(struct pg_widget *w, const struct pg_event *e, void *data);
struct pg_widget *pg_drawing_new(int width, int height, pg_draw_fn draw, pg_input_fn input, void *data);

struct pg_menu;
typedef void (*pg_menu_fn)(int index, void *data);
struct pg_menu *pg_menu_new(void);
int pg_menu_add(struct pg_menu *menu, const char *label, int icon, pg_menu_fn fn, void *data);
void pg_menu_add_separator(struct pg_menu *menu);
typedef void (*pg_menu_nav_fn)(int dir, void *data);
void pg_menu_set_shortcut(struct pg_menu *menu, int index, const char *text);
void pg_menu_set_checked(struct pg_menu *menu, int index, bool checked);
void pg_menu_on_nav(struct pg_menu *menu, pg_menu_nav_fn fn, void *data);
void pg_menu_close(struct pg_menu *menu);
bool pg_menu_is_open(const struct pg_menu *menu);
void pg_menu_set_enabled(struct pg_menu *menu, int index, bool enabled);
void pg_menu_popup(struct pg_menu *menu, struct pg_window *owner, int x, int y);
void pg_menu_destroy(struct pg_menu *menu);
