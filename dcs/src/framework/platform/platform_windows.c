#include "iplatform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

/* Phase 2.1 keeps file dialogs unparented (hwndOwner = NULL). When igraph
   is wired up in Phase 2.2 we'll let the app pass in a window handle. */

static const char *DCS_FILTER =
    "DCS Files (*.dcs)\0*.dcs\0All Files (*.*)\0*.*\0";

static int win_open_file(void *self, const char *title, char *out, int max) {
    (void)self;
    char file[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = NULL;
    ofn.lpstrFile    = file;
    ofn.nMaxFile     = sizeof(file);
    ofn.lpstrFilter  = DCS_FILTER;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle   = title;
    ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&ofn)) return 0;
    strncpy(out, file, max - 1);
    out[max - 1] = '\0';
    return 1;
}

static int win_save_file(void *self, const char *title, char *out, int max) {
    (void)self;
    char file[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = NULL;
    ofn.lpstrFile    = file;
    ofn.nMaxFile     = sizeof(file);
    ofn.lpstrFilter  = DCS_FILTER;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle   = title;
    ofn.lpstrDefExt  = "dcs";
    ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    if (!GetSaveFileNameA(&ofn)) return 0;
    strncpy(out, file, max - 1);
    out[max - 1] = '\0';
    return 1;
}

static char *win_read_file(void *self, const char *path, int *len_out) {
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

static int win_write_file(void *self, const char *path, const char *buf, int len) {
    (void)self;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t wrote = fwrite(buf, 1, (size_t)len, f);
    fclose(f);
    return (wrote == (size_t)len) ? 0 : -1;
}

static uint64_t win_time_ms(void *self) {
    (void)self;
    return (uint64_t)GetTickCount64();
}

static int win_set_clipboard(void *self, const char *text) {
    (void)self;
    if (!OpenClipboard(NULL)) return -1;
    EmptyClipboard();
    size_t len = strlen(text) + 1;
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!h) { CloseClipboard(); return -1; }
    void *dst = GlobalLock(h);
    if (!dst) { GlobalFree(h); CloseClipboard(); return -1; }
    memcpy(dst, text, len);
    GlobalUnlock(h);
    SetClipboardData(CF_TEXT, h);
    CloseClipboard();
    return 0;
}

static int win_get_clipboard(void *self, char *out, int max) {
    (void)self;
    if (!OpenClipboard(NULL)) return -1;
    HANDLE h = GetClipboardData(CF_TEXT);
    if (!h) { CloseClipboard(); return -1; }
    const char *src = (const char *)GlobalLock(h);
    if (!src) { CloseClipboard(); return -1; }
    strncpy(out, src, max - 1);
    out[max - 1] = '\0';
    GlobalUnlock(h);
    CloseClipboard();
    return 0;
}

static iplatform_t g_platform = {
    .self           = NULL,
    .open_file      = win_open_file,
    .save_file      = win_save_file,
    .read_file      = win_read_file,
    .write_file     = win_write_file,
    .time_ms        = win_time_ms,
    .set_clipboard  = win_set_clipboard,
    .get_clipboard  = win_get_clipboard,
};

iplatform_t *platform_create(void) {
    return &g_platform;
}

#endif /* _WIN32 */
