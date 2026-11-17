#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "internal.h"

struct preset
{
    const char *name;
    uint32_t desktop, window, titlebar, border, text, muted, accent, danger, shadow, surface, surface_alt,
        success, warning, info, on_accent;
};

static const struct preset presets[] = {
    {"catppuccin-dark", 0x181825, 0x1E1E2E, 0x313244, 0x45475A, 0xCDD6F4, 0x9399B2, 0x89B4FA, 0xF38BA8, 0x11111B, 0x2A2B3D, 0x45475A, 0xA6E3A1, 0xF9E2AF, 0x89DCEB, 0x11111B},
    {"nord", 0x2E3440, 0x3B4252, 0x434C5E, 0x4C566A, 0xECEFF4, 0x9AA0B0, 0x88C0D0, 0xBF616A, 0x1A1C24, 0x434C5E, 0x4C566A, 0xA3BE8C, 0xEBCB8B, 0x81A1C1, 0x2E3440},
    {"dracula", 0x282A36, 0x2E3247, 0x44475A, 0x6272A4, 0xF8F8F2, 0x9AA0B2, 0xBD93F9, 0xFF5555, 0x1A1B26, 0x383A4F, 0x44475A, 0x50FA7B, 0xF1FA8C, 0x8BE9FD, 0x282A36},
    {"light", 0xE6E9EF, 0xEFF1F5, 0xDCE0E8, 0xBCC0CC, 0x4C4F69, 0x8C8FA1, 0x1E66F5, 0xD20F39, 0x9CA0B0, 0xFFFFFF, 0xE6E9EF, 0x40A02B, 0xDF8E1D, 0x04A5E5, 0xFFFFFF},
    {"tokyo-night", 0x1A1B26, 0x24283B, 0x292E42, 0x565F89, 0xC0CAF5, 0x9AA5CE, 0x7AA2F7, 0xF7768E, 0x101014, 0x2F354F, 0x3B4261, 0x9ECE6A, 0xE0AF68, 0x7DCFFF, 0x1A1B26},
    {"gruvbox-dark", 0x1D2021, 0x282828, 0x3C3836, 0x504945, 0xEBDBB2, 0xA89984, 0xFABD2F, 0xFB4934, 0x0F0F0F, 0x32302F, 0x504945, 0xB8BB26, 0xFE8019, 0x83A598, 0x282828},
    {"one-dark", 0x21252B, 0x282C34, 0x2C313A, 0x3E4451, 0xABB2BF, 0x7F848E, 0x61AFEF, 0xE06C75, 0x15171C, 0x2F343F, 0x3E4451, 0x98C379, 0xE5C07B, 0x56B6C2, 0x21252B},
    {"rose-pine", 0x191724, 0x1F1D2E, 0x26233A, 0x403D52, 0xE0DEF4, 0x908CAA, 0xC4A7E7, 0xEB6F92, 0x0F0E17, 0x2A273F, 0x403D52, 0x9CCFD8, 0xF6C177, 0xEBBCBA, 0x191724},
    {"everforest", 0x232A2E, 0x2D353B, 0x343F44, 0x475258, 0xD3C6AA, 0x9DA9A0, 0xA7C080, 0xE67E80, 0x1A1F22, 0x3D484D, 0x475258, 0xA7C080, 0xDBBC7F, 0x7FBBB3, 0x2D353B},
    {"solarized-light", 0xEEE8D5, 0xFDF6E3, 0xEEE8D5, 0xD3CBB7, 0x586E75, 0x93A1A1, 0x268BD2, 0xDC322F, 0xB8B0A0, 0xFFFDF6, 0xEEE8D5, 0x859900, 0xB58900, 0x2AA198, 0xFDF6E3},
    {"github-dark", 0x010409, 0x0D1117, 0x161B22, 0x30363D, 0xE6EDF3, 0x8B949E, 0x2F81F7, 0xF85149, 0x000000, 0x161B22, 0x21262D, 0x3FB950, 0xD29922, 0x58A6FF, 0xFFFFFF},
};

#define PRESET_COUNT (sizeof(presets) / sizeof(presets[0]))

static struct pg_theme current;
static char source[192];
static time_t source_mtime;
static bool from_env;

static void derive(struct pg_theme *p, const struct preset *t)
{
    bool light = pgc_luma(t->window) > 140;
    memset(p, 0, sizeof(*p));
    p->name = t->name;
    p->light = light;
    p->window = t->window;
    p->bg = light ? pgc_lighten(t->window, -6) : pgc_lighten(t->window, -14);
    p->sidebar = light ? pgc_lighten(t->window, -14) : pgc_lighten(t->window, -26);
    p->titlebar = light ? pgc_lighten(t->window, -16) : pgc_lighten(t->window, -30);
    p->surface = t->surface;
    p->surface2 = pgc_mix(t->surface, t->surface_alt, 150);
    p->surface3 = t->surface_alt;
    p->border = light ? pgc_lighten(t->window, -34) : pgc_mix(p->surface3, t->window, 60);
    p->text = t->text;
    p->text2 = pgc_mix(t->text, light ? t->window : p->bg, 70);
    p->text3 = t->muted;
    p->accent = t->accent;
    p->accent2 = pgc_mix(t->accent, light ? 0x000000 : 0xffffff, 60);
    p->on_accent = t->on_accent ? t->on_accent : (pgc_luma(t->accent) > 150 ? 0x11111b : 0xffffff);
    p->danger = t->danger;
    p->success = t->success;
    p->warning = t->warning;
    p->info = t->info;
    p->shadow = t->shadow;
}

const struct pg_theme *pg_theme(void)
{
    if (!current.name)
        derive(&current, &presets[0]);
    return &current;
}

bool pg_theme_preview(const char *name, struct pg_theme *out)
{
    for (unsigned i = 0; name && out && i < PRESET_COUNT; i++)
    {
        if (strcmp(name, presets[i].name) == 0)
        {
            derive(out, &presets[i]);
            return true;
        }
    }
    return false;
}

unsigned pg_theme_count(void)
{
    return PRESET_COUNT;
}

const char *pg_theme_name(unsigned index)
{
    return index < PRESET_COUNT ? presets[index].name : presets[0].name;
}

bool pg_theme_set(const char *name)
{
    for (unsigned i = 0; name && i < PRESET_COUNT; i++)
    {
        if (strcmp(name, presets[i].name) == 0)
        {
            bool changed = current.name != presets[i].name;
            derive(&current, &presets[i]);
            if (changed)
                pg__relayout_all();
            return true;
        }
    }
    return false;
}

static bool read_theme_line(const char *path, char *out, size_t cap, time_t *mtime)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    *mtime = st.st_mtime;
    out[0] = 0;
    if (n <= 0)
        return true;
    buf[n] = 0;
    for (char *line = buf; line && *line;)
    {
        char *end = strchr(line, '\n');
        if (end)
            *end = 0;
        while (*line == ' ' || *line == '\t')
            line++;
        if (strncmp(line, "theme", 5) == 0)
        {
            char *v = line + 5;
            while (*v == ' ')
                v++;
            if (*v == '=')
            {
                v++;
                while (*v == ' ')
                    v++;
                size_t k = 0;
                while (v[k] && v[k] != '\r' && v[k] != ' ' && k + 1 < cap)
                {
                    out[k] = v[k];
                    k++;
                }
                out[k] = 0;
                return true;
            }
        }
        line = end ? end + 1 : NULL;
    }
    return true;
}

static void find_source(void)
{
    const char *home = getenv("HOME");
    source[0] = 0;
    if (home && *home)
    {
        snprintf(source, sizeof(source), "%s/.config/purec/appear.ini", home);
        struct stat st;
        if (stat(source, &st) == 0)
            return;
    }
    snprintf(source, sizeof(source), "/config/appear.ini");
}

static char system_name[48];

const char *pg_theme_system_name(void)
{
    return system_name[0] ? system_name : presets[0].name;
}

void pg_theme_reload(void)
{
    const char *env = getenv("PG_THEME");
    if (env && *env)
    {
        from_env = true;
        snprintf(system_name, sizeof(system_name), "%s", env);
        if (!pg_theme_set(env))
            pg_theme_set(presets[0].name);
        return;
    }
    find_source();
    char name[48];
    time_t mtime = 0;
    if (read_theme_line(source, name, sizeof(name), &mtime))
    {
        source_mtime = mtime;
        snprintf(system_name, sizeof(system_name), "%s", name);
    }
    else
    {
        system_name[0] = 0;
    }
    if (!system_name[0] || !pg_theme_set(system_name))
        pg_theme_set(presets[0].name);
}

void pg__theme_init(void)
{
    pg_theme_reload();
}

bool pg__theme_poll(void *data)
{
    (void)data;
    if (from_env)
        return false;
    struct stat st;
    char old[192];
    snprintf(old, sizeof(old), "%s", source);
    find_source();
    if (strcmp(old, source) != 0 || (stat(source, &st) == 0 && st.st_mtime != source_mtime))
        pg_theme_reload();
    return true;
}

uint32_t pg_color(uint32_t color)
{
    if ((color & 0xFF000000u) != PG_COLOR_ROLE)
        return color & 0xFFFFFFu;
    const struct pg_theme *t = pg_theme();
    switch (color & 0xFFu)
    {
    case 0:
        return t->text;
    case 1:
        return t->text2;
    case 2:
        return t->text3;
    case 3:
        return t->accent;
    case 4:
        return t->on_accent;
    case 5:
        return t->danger;
    case 6:
        return t->success;
    case 7:
        return t->warning;
    case 8:
        return t->info;
    case 9:
        return t->border;
    case 10:
        return t->surface;
    case 11:
        return t->surface2;
    case 12:
        return t->surface3;
    case 13:
        return t->bg;
    case 14:
        return t->sidebar;
    case 15:
        return t->window;
    default:
        return t->text;
    }
}
