#ifndef DCS_DIALOG_H
#define DCS_DIALOG_H

/* Show a native file-open dialog. On success writes the chosen path
   to out_path (null-terminated, truncated to max_len-1 chars) and
   returns 1. Returns 0 if the user cancelled or the OS call failed. */
int dialog_open_file(const char *title, char *out_path, int max_len);

/* Show a native file-save dialog. Behaves like dialog_open_file but
   prompts for overwrite confirmation and defaults the extension to .dcs. */
int dialog_save_file(const char *title, char *out_path, int max_len);

#endif /* DCS_DIALOG_H */
