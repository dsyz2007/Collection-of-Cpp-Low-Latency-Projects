#!/bin/bash
# Builds the test suites and runs every test in its own process.
#   ./run.sh [mode] [suite]
#   mode:  asan (default: ASan + UBSan, which also catches leaks), plain, or tsan
#   suite: all (default), shared (tests.cpp) or weak (weak_tests.cpp)
# To test the tests: EXTRA_FLAGS='-DSHARED_SOURCE="/path/to/header.h"' ./run.sh

cd "$(dirname "$0")" || exit 1

usage() { echo "usage: $0 [asan|plain|tsan] [all|shared|weak]"; exit 1; }

common=(-std=c++23 -Wall -Wextra -Werror -Wno-return-type -g -pthread)
case "${1:-asan}" in
    asan)  suffix=_asan; flags=(-fsanitize=address,undefined) ;;
    plain) suffix=;      flags=() ;;
    tsan)  suffix=_tsan; flags=(-fsanitize=thread) ;;
    *)     usage ;;
esac
case "${2:-all}" in
    all)    suites=(tests weak_tests) ;;
    shared) suites=(tests) ;;
    weak)   suites=(weak_tests) ;;
    *)      usage ;;
esac

suites_failed=0
for suite in "${suites[@]}"; do
    bin=./${suite}${suffix}
    echo "=== $suite.cpp (${1:-asan})"
    # shellcheck disable=SC2086
    if ! g++ "${common[@]}" "${flags[@]}" $EXTRA_FLAGS "$suite.cpp" -o "$bin"; then
        echo "BUILD FAILED"; suites_failed=$((suites_failed + 1)); continue
    fi

    passed=0; failed=0; i=0
    while IFS= read -r name; do
        out=$("$bin" "$i" 2>&1); code=$?
        summary=$(printf '%s\n' "$out" | grep -m1 '^SUMMARY:' | sed -E 's#^SUMMARY: (AddressSanitizer|ThreadSanitizer|UndefinedBehaviorSanitizer): ##')
        summary=${summary//"$PWD/"/}
        if [ "$code" -eq 0 ]; then
            printf '%2d  PASS   %s\n' "$((i + 1))" "$name"; passed=$((passed + 1))
        else
            if [ -n "$summary" ]; then status=ASAN; elif [ "$code" -eq 2 ]; then status=FAIL; else status=CRASH; fi
            printf '%2d  %-5s  %s\n' "$((i + 1))" "$status" "$name"; failed=$((failed + 1))
            printf '%s\n' "$out" | grep -E '^    (check failed|no default)'
            [ -n "$summary" ] && printf '    %s\n' "$summary"
            [ "$status" = CRASH ] && printf '    exit code %s\n' "$code"
        fi
        i=$((i + 1))
    done < <("$bin" names)
    echo "--- $passed passed, $failed failed"
    [ "$failed" -eq 0 ] || suites_failed=$((suites_failed + 1))
done
[ "$suites_failed" -eq 0 ]
