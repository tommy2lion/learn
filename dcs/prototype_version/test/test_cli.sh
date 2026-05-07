#!/bin/sh
# Integration tests for dcs_cli.exe.
# Run from project root: make test_cli

EXE=./dcs_cli.exe
PASS=0
FAIL=0

check() {
    name="$1"
    expected="$2"
    actual="$3"
    # strip CR if any (Windows line endings from MSYS pipes)
    actual=$(printf "%s" "$actual" | tr -d '\r')
    if [ "$expected" = "$actual" ]; then
        printf "PASS  %s\n" "$name"
        PASS=$((PASS + 1))
    else
        printf "FAIL  %s\n" "$name"
        printf "----- expected -----\n%s\n" "$expected"
        printf "----- actual -------\n%s\n" "$actual"
        printf "--------------------\n"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== cli integration tests ==="

# 1. NOT gate, single step
out=$($EXE circuits/not_gate.dcs --input "a=0")
check "not(0)" "a=0 -> y=1" "$out"

out=$($EXE circuits/not_gate.dcs --input "a=1")
check "not(1)" "a=1 -> y=0" "$out"

# 2. AND gate, single step (multiple-assignment format)
out=$($EXE circuits/and_gate.dcs --input "a=0,b=0")
check "and(0,0)" "a=0 b=0 -> y=0" "$out"

out=$($EXE circuits/and_gate.dcs --input "a=1,b=0")
check "and(1,0)" "a=1 b=0 -> y=0" "$out"

out=$($EXE circuits/and_gate.dcs --input "a=1,b=1")
check "and(1,1)" "a=1 b=1 -> y=1" "$out"

# 3. AND gate, single step (separate --input flags)
out=$($EXE circuits/and_gate.dcs --input "a=0" --input "b=1")
check "and(0,1) two flags" "a=0 b=1 -> y=0" "$out"

# 4. Half-adder, multi-step timing diagram
expected="step  a  b  sum  carry
   0  0  0    0      0
   1  1  0    1      0
   2  0  1    1      0
   3  1  1    0      1"
out=$($EXE circuits/half_adder.dcs --input "a=0,1,0,1" --input "b=0,0,1,1")
check "half_adder 4 steps" "$expected" "$out"

# 5. No --input flags → all undefined, output is X
out=$($EXE circuits/and_gate.dcs)
check "and no inputs -> X" "a=X b=X -> y=X" "$out"

# 6. Error: missing file
if $EXE nonexistent.dcs --input "a=0" >/dev/null 2>&1; then
    printf "FAIL  missing file should exit non-zero\n"
    FAIL=$((FAIL + 1))
else
    printf "PASS  missing file exits non-zero\n"
    PASS=$((PASS + 1))
fi

# 7. Error: parse error (unknown gate)
TMPFILE=$(mktemp -t dcs_bad.XXXXXX.dcs)
printf "inputs: a, b\noutputs: y\ny = xor(a, b)\n" > "$TMPFILE"
if $EXE "$TMPFILE" --input "a=0,b=0" >/dev/null 2>&1; then
    printf "FAIL  parse error should exit non-zero\n"
    FAIL=$((FAIL + 1))
else
    printf "PASS  parse error exits non-zero\n"
    PASS=$((PASS + 1))
fi
rm -f "$TMPFILE"

# 8. Single input compact form
out=$($EXE circuits/not_gate.dcs --input "a=1")
check "not(1) compact" "a=1 -> y=0" "$out"

echo
printf "%d passed, %d failed\n" "$PASS" "$FAIL"
exit $FAIL
