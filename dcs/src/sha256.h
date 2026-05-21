#ifndef DCS_SHA256_H
#define DCS_SHA256_H

#include <stdint.h>
#include <stddef.h>

/* Minimal SHA-256 (FIPS 180-4). Public-domain vendored implementation —
   used by R-15 part 3's build tamper signature. Pure C, zero OS / library
   dependencies, so it can be included from domain / framework / app code
   without violating any layering invariant. */

typedef struct tagt_sha256_ctx {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buf[64];
    int      bufpos;
} sha256_ctx_t;

void sha256_init  (sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len);
void sha256_final (sha256_ctx_t *ctx, uint8_t out[32]);

/* Convenience one-shot: equivalent to init + update + final. */
void sha256       (const uint8_t *data, size_t len, uint8_t out[32]);

/* Hex-encode a 32-byte hash to a 64-char lowercase string + NUL.
   out MUST point to at least 65 bytes. */
void sha256_hex   (const uint8_t hash[32], char out[65]);

#endif /* DCS_SHA256_H */
