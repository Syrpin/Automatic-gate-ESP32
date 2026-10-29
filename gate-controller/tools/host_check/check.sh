#!/usr/bin/env bash
# Host-side compile-check for gate-controller firmware sources.
# ESP-IDF is not available in this environment; this script syntax/type-checks
# all component .c files against stub headers in tools/host_check/fake/.
# Usage: tools/host_check/check.sh [gcc]
set -u
cd "$(dirname "$0")/../.." || exit 1
CC="${1:-gcc}"
FAKE="$(pwd)/tools/host_check/fake"
INC="-I$FAKE"
for d in components/*/include; do INC="$INC -I$(pwd)/$d"; done
INC="$INC -I$(pwd)/main"

fail=0
for f in main/main.c components/*/*.c; do
    out=$("$CC" -std=gnu17 -fsyntax-only -Wall -Wextra -Wno-unused-parameter \
        -Wno-sign-compare $INC "$f" 2>&1)
    rc=$?
    if [ $rc -ne 0 ]; then
        echo "=== FAIL: $f ==="
        echo "$out" | head -30
        fail=1
    else
        echo "ok: $f"
    fi
done
exit $fail
