#include "dialog.h"
#include <string.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

/* Forward-declare instead of including raylib.h here, to avoid macro
   collisions between raylib and windows.h (CloseWindow, etc.). */
extern void *GetWindowHandle(void);

static const char *DCS_FILTER = "DCS Files (*.dcs)\0*.dcs\0All Files (*.*)\0*.*\0";

int dialog_open_file(const char *title, char *out_path, int max_len) {
    char file[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = (HWND)GetWindowHandle();
    ofn.lpstrFile    = file;
    ofn.nMaxFile     = sizeof(file);
    ofn.lpstrFilter  = DCS_FILTER;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle   = title;
    ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&ofn)) return 0;
    strncpy(out_path, file, max_len - 1);
    out_path[max_len - 1] = '\0';
    return 1;
}

int dialog_save_file(const char *title, char *out_path, int max_len) {
    char file[MAX_PATH] = {0};
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = (HWND)GetWindowHandle();
    ofn.lpstrFile    = file;
    ofn.nMaxFile     = sizeof(file);
    ofn.lpstrFilter  = DCS_FILTER;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle   = title;
    ofn.lpstrDefExt  = "dcs";
    ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    if (!GetSaveFileNameA(&ofn)) return 0;
    strncpy(out_path, file, max_len - 1);
    out_path[max_len - 1] = '\0';
    return 1;
}

#else

/* Stubs for non-Windows builds — Phase 1 is Windows-only anyway. */
int dialog_open_file(const char *title, char *out_path, int max_len) {
    (void)title; (void)out_path; (void)max_len; return 0;
}
int dialog_save_file(const char *title, char *out_path, int max_len) {
    (void)title; (void)out_path; (void)max_len; return 0;
}

#endif
