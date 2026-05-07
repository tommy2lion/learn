#ifndef DCS_DOMAIN_COMPONENT_H
#define DCS_DOMAIN_COMPONENT_H

#include "../framework/core/oo.h"
#include "../framework/core/rect.h"   /* vec2_t — for component layout */
#include <stdint.h>

#define DOMAIN_NAME_LEN     64
#define DOMAIN_MAX_IO       16
#define DOMAIN_MAX_PINS_IN  2      /* primitives need at most 2; chipsets in 2.6 */

typedef uint8_t signal_t;
#define SIG_LOW    ((signal_t)0)
#define SIG_HIGH   ((signal_t)1)
#define SIG_UNDEF  ((signal_t)2)

typedef enum {
    COMP_AND, COMP_OR, COMP_NOT,
    COMP_CHIPSET,                  /* reserved — Phase 2.6+ */
} component_kind_t;

typedef struct tagt_component component_t;

class tagt_component_vt {
    component_kind_t kind;
    int  pin_count_in;
    int  pin_count_out;
    /* Compute outputs from inputs. `in` has length pin_count_in; `out` has
       length pin_count_out. Both are caller-supplied. */
    void (*evaluate)(component_t *self, const signal_t *in, signal_t *out);
    void (*destroy) (component_t *self);
};
typedef class tagt_component_vt component_vt_t;

class tagt_component {
    const component_vt_t *vt;
    char    name[DOMAIN_NAME_LEN];                       /* instance / output-wire name */
    vec2_t  position;                                    /* persisted layout (pixel coords) */
    char    in_wires[DOMAIN_MAX_PINS_IN][DOMAIN_NAME_LEN]; /* names of feeding wires */
};

/* ── primitive factories ──────────────────────────────────────────── */

component_t *gate_and_create(const char *name);
component_t *gate_or_create (const char *name);
component_t *gate_not_create(const char *name);

/* ── polymorphic helpers (safe with NULL) ─────────────────────────── */

static inline void component_destroy(component_t *c) {
    if (c && c->vt && c->vt->destroy) c->vt->destroy(c);
}
static inline component_kind_t component_kind         (const component_t *c) { return c->vt->kind;          }
static inline int               component_pin_count_in (const component_t *c) { return c->vt->pin_count_in;  }
static inline int               component_pin_count_out(const component_t *c) { return c->vt->pin_count_out; }

/* "and" / "or" / "not" / "chipset" — used by the .dcs serializer. */
const char *component_kind_name(component_kind_t k);

#endif /* DCS_DOMAIN_COMPONENT_H */
