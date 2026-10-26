#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_bin=$(mktemp "${TMPDIR:-/tmp}/stopwatch-tests.XXXXXX")
trap 'rm -f "$test_bin"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
    -fsanitize="${SANITIZERS:-undefined}" -fno-omit-frame-pointer \
    -Isrc tests/host_tests.cpp -o "$test_bin"
"$test_bin"
json_include=.pio/libdeps/m5stack-stopwatch/ArduinoJson/src
if [ ! -d "$json_include" ]; then
    echo 'Run pio run first to install the pinned ArduinoJson dependency.' >&2
    exit 1
fi
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -pedantic \
    -fsanitize="${SANITIZERS:-undefined}" -fno-omit-frame-pointer \
    -Isrc -I"$json_include" tests/api_tests.cpp -o "$test_bin"
"$test_bin"
