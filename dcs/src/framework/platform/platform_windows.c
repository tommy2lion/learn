#include "iplatform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

/* File-dialog owner: when callers pass the raylib HWND via the new
   `owner` parameter, the dialog centres on our window and is window-
   modal. NULL falls back to unparented (screen-centred). */

static const char *DCS_FILTER =
    "DCS Files (*.dcs)\0*.dcs\0All Files (*.*)\0*.*\0";

static int win_open_file(void *self, void *owner, const char *title, char *out, int max) {
    (void)self;
    char file[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = (HWND)owner;
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

static int win_save_file(void *self, void *owner, const char *title, char *out, int max) {
    (void)self;
    char file[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = (HWND)owner;
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

/* MessageBoxA does NOT automatically centre on the owner HWND on any
   Windows version we care about — it screen-centres regardless of
   whether hWnd is set. The well-known Win32 workaround is a thread-
   local CBT hook that catches the dialog's HCBT_ACTIVATE event and
   repositions it. (Alternative: use TaskDialog, but that adds a v6
   common-controls dependency and a manifest, more invasive than this
   ~20-line hook.) */
static HHOOK g_centre_hook       = NULL;
static HWND  g_centre_dialog_owner = NULL;

static LRESULT CALLBACK centre_cbt_proc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HCBT_ACTIVATE && g_centre_dialog_owner) {
        HWND dlg = (HWND)wParam;
        RECT owner_rect, dlg_rect;
        if (GetWindowRect(g_centre_dialog_owner, &owner_rect) &&
            GetWindowRect(dlg, &dlg_rect)) {
            int ow = owner_rect.right  - owner_rect.left;
            int oh = owner_rect.bottom - owner_rect.top;
            int dw = dlg_rect.right    - dlg_rect.left;
            int dh = dlg_rect.bottom   - dlg_rect.top;
            int x  = owner_rect.left + (ow - dw) / 2;
            int y  = owner_rect.top  + (oh - dh) / 2;
            SetWindowPos(dlg, NULL, x, y, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    return CallNextHookEx(g_centre_hook, code, wParam, lParam);
}

static dialog_result_t win_confirm_yes_no_cancel(void *self,
                                                 void *owner,
                                                 const char *title,
                                                 const char *message) {
    (void)self;
    UINT flags = MB_YESNOCANCEL | MB_ICONQUESTION;
    if (owner) {
        g_centre_dialog_owner = (HWND)owner;
        g_centre_hook = SetWindowsHookExA(WH_CBT, centre_cbt_proc,
                                          NULL, GetCurrentThreadId());
    }
    int r = MessageBoxA((HWND)owner,
                        message ? message : "", title ? title : "", flags);
    if (g_centre_hook) {
        UnhookWindowsHookEx(g_centre_hook);
        g_centre_hook         = NULL;
        g_centre_dialog_owner = NULL;
    }
    switch (r) {
        case IDYES:    return DLG_YES;
        case IDNO:     return DLG_NO;
        case IDCANCEL: return DLG_CANCEL;
        default:       return DLG_ERROR;
    }
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
    .confirm_yes_no_cancel = win_confirm_yes_no_cancel,
};

iplatform_t *platform_create(void) {
    return &g_platform;
}

#endif /* _WIN32 */
