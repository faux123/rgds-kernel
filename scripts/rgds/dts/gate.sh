#!/bin/sh
# Usage: gate.sh STOCK.dtb BUILT.dtb
# Prints the number of semantic diff lines (0 = equivalent) after resolving phandle references.
d=$(dirname "$0")
python3 -I "$d/norm.py" "$1" > "${TMPDIR:-/tmp}/rgds-gate-stock.txt" || exit 2
python3 -I "$d/norm.py" "$2" > "${TMPDIR:-/tmp}/rgds-gate-built.txt" || exit 2
diff "${TMPDIR:-/tmp}/rgds-gate-stock.txt" "${TMPDIR:-/tmp}/rgds-gate-built.txt" | tee "${TMPDIR:-/tmp}/rgds-gate.diff" | grep -c '^[<>]'
