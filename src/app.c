#include <poll.h>
#include <time.h>
#include "internal.h"

struct pg_timer {
    struct pg_timer *next;
    uint32_t id;
    uint32_t interval;
    uint64_t due;
    pg_timer_fn fn;
    void *data;
    bool dead;
};

struct pg_watch {
    struct pg_watch *next;
    uint32_t id;
    int fd;
    uint32_t events;
    pg_fd_fn fn;
    void *data;
    bool dead;
};

static struct {
    bool ready;
    bool running;
    int code;
    uint32_t next_id;
    struct pg_timer *timers;
    struct pg_watch *watches;
    uint32_t theme_timer;
    bool hold;
} app;

const char *pg_version(void)
{
    return "2.2.0";
}

char *pg__strdup(const char *s)
{
    if (!s)
        s = "";
    size_t n = strlen(s);
    char *d = malloc(n + 1);
    if (d)
        memcpy(d, s, n + 1);
    return d;
}

void pg__set_text(char **slot, const char *s)
{
    char *d = pg__strdup(s);
    if (!d)
        return;
    free(*slot);
    *slot = d;
}

uint64_t pg_time_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

bool pg_init(int argc, char **argv)
{
    if (app.ready)
        return true;
    for (int i = 1; argv && i < argc; i++) {
        if (strcmp(argv[i], "--theme") == 0 && i + 1 < argc)
            setenv("PG_THEME", argv[++i], 1);
    }
    if (vb_connect() < 0)
        return false;
    pg__theme_init();
    app.ready = true;
    app.theme_timer = pg_timer_add(1500, pg__theme_poll, NULL);
    return true;
}

void pg_shutdown(void)
{
    if (!app.ready)
        return;
    for (struct pg_window *w = pg__windows(); w; w = pg__windows()) {
        pg_window_destroy(w);
        pg__reap();
    }
    while (app.timers) {
        struct pg_timer *t = app.timers;
        app.timers = t->next;
        free(t);
    }
    while (app.watches) {
        struct pg_watch *w = app.watches;
        app.watches = w->next;
        free(w);
    }
    vb_disconnect();
    app.ready = false;
}

void pg_hold(bool hold)
{
    app.hold = hold;
}

bool pg_running(void)
{
    return app.running;
}

void pg_quit(int code)
{
    app.code = code;
    app.running = false;
}

uint32_t pg_timer_add(uint32_t ms, pg_timer_fn fn, void *data)
{
    if (!fn)
        return 0;
    struct pg_timer *t = calloc(1, sizeof(*t));
    if (!t)
        return 0;
    t->id = ++app.next_id;
    t->interval = ms;
    t->due = pg_time_ms() + ms;
    t->fn = fn;
    t->data = data;
    t->next = app.timers;
    app.timers = t;
    return t->id;
}

void pg_timer_remove(uint32_t id)
{
    for (struct pg_timer *t = app.timers; t; t = t->next)
        if (t->id == id)
            t->dead = true;
}

uint32_t pg_fd_watch(int fd, uint32_t events, pg_fd_fn fn, void *data)
{
    if (fd < 0 || !fn)
        return 0;
    struct pg_watch *w = calloc(1, sizeof(*w));
    if (!w)
        return 0;
    w->id = ++app.next_id;
    w->fd = fd;
    w->events = events ? events : PG_FD_READ;
    w->fn = fn;
    w->data = data;
    w->next = app.watches;
    app.watches = w;
    return w->id;
}

void pg_fd_unwatch(uint32_t id)
{
    for (struct pg_watch *w = app.watches; w; w = w->next)
        if (w->id == id)
            w->dead = true;
}

void pg_screen_size(int *width, int *height)
{
    vb_screen_size(width, height);
}

void pg_work_area(int *x, int *y, int *width, int *height)
{
    vb_work_area(x, y, width, height);
}

static void sweep(void)
{
    struct pg_timer **pt = &app.timers;
    while (*pt) {
        if ((*pt)->dead) {
            struct pg_timer *d = *pt;
            *pt = d->next;
            free(d);
        } else {
            pt = &(*pt)->next;
        }
    }
    struct pg_watch **pw = &app.watches;
    while (*pw) {
        if ((*pw)->dead) {
            struct pg_watch *d = *pw;
            *pw = d->next;
            free(d);
        } else {
            pw = &(*pw)->next;
        }
    }
}

static void run_timers(void)
{
    uint64_t now = pg_time_ms();
    for (struct pg_timer *t = app.timers; t; t = t->next) {
        if (t->dead || t->due > now)
            continue;
        if (t->fn(t->data))
            t->due = now + (t->interval ? t->interval : 1);
        else
            t->dead = true;
    }
    sweep();
}

static int next_timeout(void)
{
    uint64_t now = pg_time_ms();
    int64_t best = -1;
    for (struct pg_timer *t = app.timers; t; t = t->next) {
        if (t->dead)
            continue;
        int64_t left = t->due > now ? (int64_t)(t->due - now) : 0;
        if (best < 0 || left < best)
            best = left;
    }
    if (best > 1000)
        best = 1000;
    return (int)best;
}

static bool dispatch_server(void)
{
    struct vb_event e;
    for (;;) {
        int r = vb_next_event(&e, 0);
        if (r < 0)
            return false;
        if (r == 0)
            return true;
        if (e.type == VB_EVENT_SCREEN || e.type == VB_EVENT_WORKAREA) {
            pg__relayout_all();
            continue;
        }
        struct pg_window *win = pg__window_of(e.window);
        if (win && !win->dead)
            pg__window_event(win, &e);
    }
}

int pg_run(void)
{
    if (!app.ready && !pg_init(0, NULL))
        return 1;
    app.running = true;
    app.code = 0;
    while (app.running) {
        if (!dispatch_server()) {
            app.code = 1;
            break;
        }
        run_timers();
        pg__flush_all();
        pg__reap();
        if (!app.running || (!app.hold && !pg__have_windows()))
            break;
        struct pollfd fds[32];
        struct pg_watch *map[32];
        int n = 0;
        fds[n].fd = vb_fd();
        fds[n].events = POLLIN;
        fds[n].revents = 0;
        map[n++] = NULL;
        for (struct pg_watch *w = app.watches; w && n < 32; w = w->next) {
            if (w->dead)
                continue;
            fds[n].fd = w->fd;
            fds[n].events = (short)(((w->events & PG_FD_READ) ? POLLIN : 0) | ((w->events & PG_FD_WRITE) ? POLLOUT : 0));
            fds[n].revents = 0;
            map[n++] = w;
        }
        int r = poll(fds, (nfds_t)n, next_timeout());
        if (r <= 0)
            continue;
        if (fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
            app.code = 1;
            break;
        }
        for (int i = 1; i < n; i++) {
            if (!fds[i].revents || map[i]->dead)
                continue;
            uint32_t ev = 0;
            if (fds[i].revents & (POLLIN | POLLHUP | POLLERR))
                ev |= PG_FD_READ;
            if (fds[i].revents & POLLOUT)
                ev |= PG_FD_WRITE;
            map[i]->fn(map[i]->fd, ev, map[i]->data);
        }
        sweep();
    }
    app.running = false;
    pg__reap();
    return app.code;
}
