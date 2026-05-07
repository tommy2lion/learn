#include "iplatform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef _WIN32

#include <time.h>

/* Phase 2.1 stub: file I/O and time work; dialogs and clipboard are no-ops
   until Phase 2.7 wires up zenity (with graceful fallback when missing). */

static int lnx_open_file(void *self, const char *title, char *out, int max) {
    (void)self; (void)title; (void)out; (void)max;
    return 0;
}

static int lnx_save_file(void *self, const char *title, char *out, int max) {
    (void)self; (void)title; (void)out; (void)max;
    return 0;
}

static char *lnx_read_file(void *self, const char *path, int *len_out) {
    (void)self;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)len, f);
    buf[got] = '\0';
    fclose(f);
    if (len_out) *len_out = (int)got;
    return buf;
}

static int lnx_write_file(void *self, const char *path, const char *buf, int len) {
    (void)self;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t wrote = fwrite(buf, 1, (size_t)len, f);
    fclose(f);
    return (wrote == (size_t)len) ? 0 : -1;
}

static uint64_t lnx_time_ms(void *self) {
    (void)self;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

static int lnx_set_clipboard(void *self, const char *text) {
    (void)self; (void)text;
    return 0;
}

static int lnx_get_clipboard(void *self, char *out, int max) {
    (void)self; (void)out; (void)max;
    return -1;
}

static iplatform_t g_platform = {
    .self           = NULL,
    .open_file      = lnx_open_file,
    .save_file      = lnx_save_file,
    .read_file      = lnx_read_file,
    .write_file     = lnx_write_file,
    .time_ms        = lnx_time_ms,
    .set_clipboard  = lnx_set_clipboard,
    .get_clipboard  = lnx_get_clipboard,
};

iplatform_t *platform_create(void) {
    return &g_platform;
}

#endif /* !_WIN32 */
