#include "../src/framework/platform/iplatform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0, total = 0;

static void check(const char *name, int cond) {
    total++;
    printf("%s  %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) failures++;
}

int main(void) {
    printf("=== iplatform unit tests ===\n");

    iplatform_t *p = platform_create();
    check("platform_create non-NULL", p != NULL);
    if (!p) { printf("\n0 / 1 passed\n"); return 1; }

    /* ── time ──────────────────────────────────────────────── */
    uint64_t t0 = p->time_ms(p->self);
    check("time_ms returns non-zero", t0 > 0);
    uint64_t t1 = p->time_ms(p->self);
    check("time_ms is non-decreasing", t1 >= t0);

    /* ── round-trip a small file ───────────────────────────── */
    const char *path = "test_iplatform_roundtrip.tmp";
    const char *text = "Hello, iplatform.\nLine 2.\nLast line.";
    int n = (int)strlen(text);

    int wr = p->write_file(p->self, path, text, n);
    check("write_file succeeds", wr == 0);

    int len = -1;
    char *buf = p->read_file(p->self, path, &len);
    check("read_file returns buffer", buf != NULL);
    check("read_file length matches", len == n);
    if (buf) {
        check("read_file content matches", strcmp(buf, text) == 0);
        free(buf);
    }
    remove(path);

    /* ── missing file ──────────────────────────────────────── */
    char *miss = p->read_file(p->self, "this-file-should-not-exist.tmp", NULL);
    check("read_file missing -> NULL", miss == NULL);

    /* ── write to a path with no directory should still work
           (current dir); negative-path test is OS-dependent so skip ── */

    printf("\n%d / %d passed\n", total - failures, total);
    return failures;
}
