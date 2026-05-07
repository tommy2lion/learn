#ifndef DCS_PARSER_H
#define DCS_PARSER_H

#include "sim.h"

/* Parse .dcs text into a new Circuit.
   Returns NULL on error; writes a message to err_out (if non-NULL, err_len > 0). */
Circuit *parse_circuit(const char *text, char *err_out, int err_len);

/* Serialize a Circuit to .dcs text.
   Caller must free the returned string. Returns NULL on allocation failure. */
char *serialize_circuit(const Circuit *c);

#endif /* DCS_PARSER_H */
