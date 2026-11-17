# RG DS kernel reconstruction notes

This branch holds the evidence and tools behind the RG DS V1.21 kernel reconstruction. The kernel
source lives on the other two branches:

- `rgds-v1.21-stock`: source rebuilt to match Anbernic's stock V1.21 kernel (6.1.141).
- `rgds-v1.21`: the stock branch plus three visible fixes.

## What is here

- `RECON_NOTES.md`: the working log. Every reconstruction step, the stock addresses it was checked
  against, and the final list of functions that still differ from stock, each with its reason.
- `audit/`: the function-by-function comparison against the stock kernel.
  - `allcmp2.py` classifies every function as identical, address-only, same size with different
    code, or different size.
  - `final-nonidentical.tsv` lists every function that is not identical or address-only, with its
    class.
  - `merge-resolution*.tsv` and `conflicts.tsv` record how each Android common kernel merge conflict
    was resolved.
  - `stock-modules.txt` lists every module the stock firmware ships.
- `symdiff/`: symbols found only in stock or only in our build, for both branches.
- `tools/function-compare/`: per-function and per-object disassembly comparison scripts.
- `tools/hynitron/`: comparison scripts used for the Hynitron touchscreen driver.
- `build/`: the build scripts used for every comparison build.

The scripts still carry the absolute paths of the machine they ran on; adjust them before use.

## What this proves, and what it does not

The source is a reconstruction checked against the stock binary, not Anbernic's original files.
Comments, code compiled out of stock, and exact line positions cannot be recovered from a binary.
`RECON_NOTES.md` lists the functions that still differ and why.
