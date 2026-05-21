/* SHA-256 tests against the FIPS 180-4 reference vectors plus a couple
   of edge cases (long input that crosses the 64-byte block boundary;
   streaming vs one-shot equivalence). */

#include "../src/sha256.h"
#include <stdio.h>
#include <string.h>

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

static int hash_equals(const char *input, const char *expected_hex) {
    uint8_t h[32];
    char    hex[65];
    sha256((const uint8_t *)input, strlen(input), h);
    sha256_hex(h, hex);
    return strcmp(hex, expected_hex) == 0;
}

int main(void) {
    printf("=== sha256 unit tests ===\n");

    /* ── FIPS 180-4 reference vectors ────────────────────────── */
    check("sha256(\"\")",
          hash_equals("",
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    check("sha256(\"abc\")",
          hash_equals("abc",
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    check("sha256(56-char block)",
          hash_equals("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));

    /* ── input that crosses the 64-byte block boundary ───────── */
    {
        /* 1 million 'a' chars: well-known NIST long-input vector. Build it
           in chunks to also exercise streaming update. */
        uint8_t  out[32];
        char     hex[65];
        sha256_ctx_t ctx;
        sha256_init(&ctx);
        uint8_t chunk[1000];
        memset(chunk, 'a', sizeof(chunk));
        /* 1000 × 1000 = 1,000,000 bytes — matches the NIST long-input vector. */
        for (int i = 0; i < 1000; i++) sha256_update(&ctx, chunk, sizeof(chunk));
        sha256_final(&ctx, out);
        sha256_hex(out, hex);
        check("sha256(1e6 × 'a') matches NIST vector",
              strcmp(hex,
                "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0);
    }

    /* ── streaming equivalence: split a string at every offset and
           confirm we always get the same hash as the one-shot. ── */
    {
        const char *input = "the quick brown fox jumps over the lazy dog";
        int len = (int)strlen(input);
        uint8_t expected[32];
        sha256((const uint8_t *)input, (size_t)len, expected);

        int all_match = 1;
        for (int split = 0; split <= len; split++) {
            sha256_ctx_t ctx;
            sha256_init(&ctx);
            sha256_update(&ctx, (const uint8_t *)input, (size_t)split);
            sha256_update(&ctx, (const uint8_t *)input + split, (size_t)(len - split));
            uint8_t out[32];
            sha256_final(&ctx, out);
            if (memcmp(out, expected, 32) != 0) { all_match = 0; break; }
        }
        check("streaming update at every split == one-shot hash", all_match);
    }

    /* ── sha256_hex round-trip ─────────────────────────────── */
    {
        uint8_t hash[32];
        for (int i = 0; i < 32; i++) hash[i] = (uint8_t)i;
        char hex[65];
        sha256_hex(hash, hex);
        check("sha256_hex preserves byte order",
              strcmp(hex,
                "000102030405060708090a0b0c0d0e0f"
                "101112131415161718191a1b1c1d1e1f") == 0);
        check("sha256_hex NUL-terminates", hex[64] == '\0');
    }

    printf("\n%d / %d passed\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
