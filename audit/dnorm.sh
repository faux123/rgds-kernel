#!/bin/sh
# Normalize Ghidra pseudocode so two builds compare: drop address-derived names.
sed -E 's/(DAT|PTR|FUN|LAB|switchD|caseD|s|u)_[0-9a-fA-F]{6,}(_[0-9a-f]+)?/\1_X/g; s/0x[0-9a-f]{8,}/ADDR/g' "$1"
