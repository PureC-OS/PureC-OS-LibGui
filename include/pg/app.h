#pragma once
#include <stdbool.h>
#include <stdint.h>

#define PG_VERSION_MAJOR 2
#define PG_VERSION_MINOR 1
#define PG_VERSION_PATCH 0

#define PG_FD_READ 1u
#define PG_FD_WRITE 4u

typedef bool (*pg_timer_fn)(void *data);
typedef void (*pg_fd_fn)(int fd, uint32_t events, void *data);

const char *pg_version(void);
bool pg_init(int argc, char **argv);
int pg_run(void);
void pg_quit(int code);
void pg_shutdown(void);
bool pg_running(void);
void pg_hold(bool hold);

uint64_t pg_time_ms(void);
uint32_t pg_timer_add(uint32_t ms, pg_timer_fn fn, void *data);
void pg_timer_remove(uint32_t id);
uint32_t pg_fd_watch(int fd, uint32_t events, pg_fd_fn fn, void *data);
void pg_fd_unwatch(uint32_t id);

void pg_screen_size(int *width, int *height);
void pg_work_area(int *x, int *y, int *width, int *height);
