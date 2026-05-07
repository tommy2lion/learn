#ifndef DCS_DOMAIN_CIRCUIT_IO_H
#define DCS_DOMAIN_CIRCUIT_IO_H

#include "circuit.h"

/* Parse .dcs text into a new circuit. Returns NULL on error and writes
   a message to err_out (if non-NULL, err_len > 0). */
circuit_t *circuit_io_parse(const char *text, char *err_out, int err_len);

/* Serialize a circuit to .dcs text. Caller must free the returned string.
   Returns NULL on allocation failure. */
char *circuit_io_serialize(const circuit_t *c);

#endif /* DCS_DOMAIN_CIRCUIT_IO_H */
