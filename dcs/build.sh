#!/usr/bin/env bash
# build.sh — one-shot build for both dcs_cli.exe and dcs_gui.exe.
#
# Usage:
#   ./build.sh              # release build (-O2)        — default
#   ./build.sh release      # release build (-O2)        — explicit
#   ./build.sh debug        # debug build (-g -O0 -DDEBUG)
#   ./build.sh clean        # remove build artefacts
#   ./build.sh test         # run the full test suite
#   ./build.sh -h           # this help
#
# Reads DCS_BUILD_SALT from `.env` if present (see .env.example).
# Override on the command line:  ./build.sh release  (then) make DCS_BUILD_SALT=x cli
# or copy/edit .env per the .env.example documentation.
#
# Pure wrapper around `make` — does nothing the Makefile can't do directly.

set -euo pipefail

mode="${1:-release}"
# State file remembers the last successful build mode. On mode switch we
# pass -B so make recompiles everything against the new CFLAGS (CFLAGS
# values aren't in make's dependency graph, so an incremental build would
# silently keep the previous mode's objects).
STATE_FILE=".build_mode"
last_mode="$(cat "$STATE_FILE" 2>/dev/null || true)"
force_flag=""
if [ "$mode" != "$last_mode" ] && [ "$mode" != "clean" ] && [ "$mode" != "test" ] \
   && [ "$mode" != "-h" ] && [ "$mode" != "--help" ] && [ "$mode" != "help" ]; then
    force_flag="-B"
fi

print_help() {
    sed -n '2,17p' "$0"
}

print_built() {
    echo
    echo "==> built (mode: $mode)"
    ls -lh dcs_cli.exe dcs_gui.exe 2>/dev/null || true
    echo
    if [ -x ./dcs_cli.exe ]; then ./dcs_cli.exe --version; fi
}

case "$mode" in
    release)
        echo "==> release build${force_flag:+ ($force_flag — mode changed since last build)}"
        make $force_flag cli
        make $force_flag gui
        echo "$mode" > "$STATE_FILE"
        print_built
        ;;
    debug)
        echo "==> debug build${force_flag:+ ($force_flag — mode changed since last build)}"
        DBG='CFLAGS=-Wall -Wextra -g -O0 -std=c99 -DDEBUG'
        make $force_flag "$DBG" cli
        make $force_flag "$DBG" gui
        echo "$mode" > "$STATE_FILE"
        print_built
        ;;
    clean)
        make clean
        rm -f "$STATE_FILE"
        ;;
    test)
        make test
        ;;
    -h|--help|help)
        print_help
        ;;
    *)
        echo "error: unknown mode '$mode'" >&2
        echo "usage: $0 [release|debug|clean|test|-h]" >&2
        exit 1
        ;;
esac
