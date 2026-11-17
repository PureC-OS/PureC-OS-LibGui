#pragma once
#include <stdbool.h>
#include <stdint.h>

struct pg_theme {
    const char *name;
    bool light;
    uint32_t window;
    uint32_t bg;
    uint32_t sidebar;
    uint32_t titlebar;
    uint32_t surface;
    uint32_t surface2;
    uint32_t surface3;
    uint32_t border;
    uint32_t text;
    uint32_t text2;
    uint32_t text3;
    uint32_t accent;
    uint32_t accent2;
    uint32_t on_accent;
    uint32_t danger;
    uint32_t success;
    uint32_t warning;
    uint32_t info;
    uint32_t shadow;
};

#define PG_COLOR_ROLE 0x01000000u
#define PG_COLOR_TEXT (PG_COLOR_ROLE | 0u)
#define PG_COLOR_TEXT2 (PG_COLOR_ROLE | 1u)
#define PG_COLOR_TEXT3 (PG_COLOR_ROLE | 2u)
#define PG_COLOR_ACCENT (PG_COLOR_ROLE | 3u)
#define PG_COLOR_ON_ACCENT (PG_COLOR_ROLE | 4u)
#define PG_COLOR_DANGER (PG_COLOR_ROLE | 5u)
#define PG_COLOR_SUCCESS (PG_COLOR_ROLE | 6u)
#define PG_COLOR_WARNING (PG_COLOR_ROLE | 7u)
#define PG_COLOR_INFO (PG_COLOR_ROLE | 8u)
#define PG_COLOR_BORDER (PG_COLOR_ROLE | 9u)
#define PG_COLOR_SURFACE (PG_COLOR_ROLE | 10u)
#define PG_COLOR_SURFACE2 (PG_COLOR_ROLE | 11u)
#define PG_COLOR_SURFACE3 (PG_COLOR_ROLE | 12u)
#define PG_COLOR_BG (PG_COLOR_ROLE | 13u)
#define PG_COLOR_SIDEBAR (PG_COLOR_ROLE | 14u)
#define PG_COLOR_WINDOW (PG_COLOR_ROLE | 15u)

const struct pg_theme *pg_theme(void);
bool pg_theme_set(const char *name);
bool pg_theme_preview(const char *name, struct pg_theme *out);
const char *pg_theme_system_name(void);
void pg_theme_reload(void);
unsigned pg_theme_count(void);
const char *pg_theme_name(unsigned index);
uint32_t pg_color(uint32_t color);
