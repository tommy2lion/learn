#ifndef DCS_DOMAIN_CIRCUIT_IO_H
#define DCS_DOMAIN_CIRCUIT_IO_H

#include "circuit.h"
#include "wire_geometry.h"

/* Parse .dcs text into a new circuit. Returns NULL on error and writes
   a message to err_out (if non-NULL, err_len > 0). */
circuit_t *circuit_io_parse(const char *text, char *err_out, int err_len);

/* Serialize a circuit to .dcs text. Caller must free the returned string.
   Returns NULL on allocation failure. */
char *circuit_io_serialize(const circuit_t *c);

/* Extended parse: also extracts the "# @wires" annotation block (if present)
   into geom_out. Pass geom_out=NULL to ignore the wires block (equivalent
   to the legacy circuit_io_parse). geom_out must be initialized before the
   call (callers typically zero-init or wire_geometry_init it). */
circuit_t *circuit_io_parse_ex(const char *text, char *err_out, int err_len,
                               wire_geometry_t *geom_out);

/* Extended serialize: also emits a "# @wires" annotation block when `geom`
   is non-NULL and contains at least one net with segments. Pass geom=NULL
   for the legacy output format. */
char *circuit_io_serialize_ex(const circuit_t *c, const wire_geometry_t *geom);

#endif /* DCS_DOMAIN_CIRCUIT_IO_H */
