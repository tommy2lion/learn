#ifndef DCS_BUILD_INFO_H
#define DCS_BUILD_INFO_H

/* Build-time metadata baked in by the Makefile (R-15 part 2). The body
   lives in src/build_info.c which is auto-generated on every build and
   intentionally NOT committed — the date and git short hash change with
   every checkout. See the build-info rule in Makefile. */

extern const char *dcs_build_date;     /* YYYYMMDD, e.g. "20260521" */
extern const char *dcs_build_commit;   /* 8-char git short, or "unknown" */

#endif /* DCS_BUILD_INFO_H */
