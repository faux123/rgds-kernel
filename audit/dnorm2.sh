#!/bin/sh
# Stronger normalization: also rename Ghidra temporaries and drop the '_' overlap prefix.
../dnorm.sh "$1" | sed -E 's/\b[a-z]*Var[0-9]+\b/V/g; s/\b_([a-z])/\1/g; s/uRamffffffc00a4d[0-9a-f]{4}/TOUCH/g; s/touch_data\._[0-9]+_[0-9]+_/TOUCH/g; s/\(undefined4 \*\)//g' | grep -v '^\s*$' | grep -v WARNING
