#ifndef DCS_FW_IPLATFORM_H
#define DCS_FW_IPLATFORM_H

#include "../core/oo.h"
#include <stdint.h>

/* iplatform: thin wrapper around OS-specific services so application code
   never includes windows.h / linux headers directly. Concrete implementation
   is selected at link time (platform_windows.c on Windows, platform_linux.c
   elsewhere). */

/* Result of a 3-button confirmation dialog. Numeric values intentionally
   match the natural Yes/No/Cancel ordering; DLG_ERROR is negative so a
   caller can `if (r < 0)` to detect "no dialog was shown". (R-11) */
typedef enum {
    DLG_YES    =  0,
    DLG_NO     =  1,
    DLG_CANCEL =  2,
    DLG_ERROR  = -1,
} dialog_result_t;

interface tagt_iplatform {
    void *self;

    /* file dialogs: return 1 on success (path written to out), 0 on cancel/error */
    int   (*open_file)  (void *self, const char *title, char *out, int max);
    int   (*save_file)  (void *self, const char *title, char *out, int max);

    /* Show a modal 3-button confirmation dialog. `owner` is the native
       OS window handle (typically from igraph_t::get_native_window_handle)
       used to centre the dialog on the program's window and make it
       window-modal; NULL falls back to screen-centred + application-modal.
       Button labels are platform-defined (Windows: Yes / No / Cancel);
       phrase the message so those map naturally (e.g. "Save changes
       before closing?" → Yes saves, No discards, Cancel aborts).
       Returns one of dialog_result_t. (R-11) */
    dialog_result_t (*confirm_yes_no_cancel)(void *self,
                                             void *owner,
                                             const char *title,
                                             const char *message);

    /* file I/O — synchronous, small-file friendly (whole-file in/out).
       Streaming/chunked APIs can be added later when waveform export
       or chipset libraries grow. Caller frees the read_file buffer. */
    char *(*read_file)  (void *self, const char *path, int *len_out);
    int   (*write_file) (void *self, const char *path, const char *buf, int len);

    /* monotonic millisecond clock since some unspecified epoch */
    uint64_t (*time_ms) (void *self);

    /* clipboard — may be a no-op on platforms that don't support it */
    int   (*set_clipboard)(void *self, const char *text);
    int   (*get_clipboard)(void *self, char *out, int max);
};
typedef interface tagt_iplatform iplatform_t;

/* Returns the platform singleton for this build, or NULL on unsupported platforms. */
iplatform_t *platform_create(void);

#endif /* DCS_FW_IPLATFORM_H */
