#ifndef DCS_DOMAIN_CIRCUIT_IO_H
#define DCS_DOMAIN_CIRCUIT_IO_H

#include "circuit.h"
#include "wire_geometry.h"

/* Persistable subset of the app-layer external_view_metadata_t. Plain
   data — domain-friendly (no UI / raylib / function pointer deps). The
   app layer marshals between this and its richer metadata struct.
   Phase 10. */
typedef struct tagt_circuit_meta {
    /* 0 == DISPLAY_INTERNAL (default), 1 == DISPLAY_EXTERNAL */
    int  display_mode;
    /* Label drawn inside the external-view box; empty == no override
       (renderer falls back to whatever the app supplies, typically the
       file basename). */
    char display_name[DOMAIN_NAME_LEN];
    /* Per-pin style override: 0 == NORMAL (default), 1 == CLOCK,
       2 == INVERTED. Matches pin_style_t in app/external_view.h. */
    int  input_styles [DOMAIN_MAX_IO];
    int  output_styles[DOMAIN_MAX_IO];
} circuit_meta_t;

/* Parse .dcs text into a new circuit. Returns NULL on error and writes
   a message to err_out (if non-NULL, err_len > 0). */
circuit_t *circuit_io_parse(const char *text, char *err_out, int err_len);

/* Serialize a circuit to .dcs text. Caller must free the returned string.
   Returns NULL on allocation failure. */
char *circuit_io_serialize(const circuit_t *c);

/* Extended parse: also extracts the "# @wires" annotation block (if present)
   into geom_out, and the # @display_mode / # @display_name / # @pin_style
   single-line annotations into meta_out. Either out-param may be NULL.
   Both must be initialized (typically zero-init or wire_geometry_init)
   before the call. */
circuit_t *circuit_io_parse_ex(const char *text, char *err_out, int err_len,
                               wire_geometry_t *geom_out,
                               circuit_meta_t  *meta_out);

/* Extended serialize: also emits a "# @wires" annotation block when `geom`
   is non-NULL with segments, and emits "# @display_mode" / "# @display_name"
   / "# @pin_style" lines for the non-default fields of meta. Either may be
   NULL to skip that subset. */
char *circuit_io_serialize_ex(const circuit_t *c,
                              const wire_geometry_t *geom,
                              const circuit_meta_t  *meta);

#endif /* DCS_DOMAIN_CIRCUIT_IO_H */
