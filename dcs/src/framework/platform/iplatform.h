#ifndef DCS_FW_IPLATFORM_H
#define DCS_FW_IPLATFORM_H

#include "../core/oo.h"
#include <stdint.h>

/* iplatform: thin wrapper around OS-specific services so application code
   never includes windows.h / linux headers directly. Concrete implementation
   is selected at link time (platform_windows.c on Windows, platform_linux.c
   elsewhere). */

interface tagt_iplatform {
    void *self;

    /* file dialogs: return 1 on success (path written to out), 0 on cancel/error */
    int   (*open_file)  (void *self, const char *title, char *out, int max);
    int   (*save_file)  (void *self, const char *title, char *out, int max);

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
