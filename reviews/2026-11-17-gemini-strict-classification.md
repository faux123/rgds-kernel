### CRITICAL

- **Offset collision masking in `norm()`**: In `strict_audit.py` (lines 142–146), any load/store using an adrp-derived base register has its offset replaced with `#LO]`, regardless of value (`re.sub(r'#0x[0-9a-f]+\]', '#LO]', arg)` and `re.sub(r'\[(x\d+)\]', r'[\1, #LO]', arg)`). 
  - If stock loads `[x0, #0x10]` and your build loads `[x0, #0x28]` (accessing a completely different field or array index within the same page), both normalize to `[x0, #LO]`.
  - Offset `0` (`[x0]`) and non-zero offset `[x0, #0x800]` normalize to the exact same string `[x0, #LO]`. 
  - This hides source-level bugs, struct member mismatches, and global array indexing bugs within `STRICT_SAME`.

- **Flow-insensitive `STRICT_SAME` evaluation**: In `strict_audit.py` (lines 125–158), when `flow_insensitive=False`, `page` is tracked across a naive top-to-bottom linear scan of the disassembly without following CFG edges.
  - If `adrp x0` occurs in a conditional block or prior to an early exit, `x0` remains in `page` for all subsequent instructions in the function until overwritten.
  - If `x0` is later loaded with a struct pointer (`ldr x0, [sp, #16]`), `x0` is removed from `page` because `ldr` is not in `NO_DEST`. However, if another register (e.g., callee-saved `x19`) is loaded via `adrp x19, <page>` at function start, any subsequent access `ldr x1, [x19, #struct_off]` anywhere in the function will have its struct offset wiped to `#LO]`.

- **ADDR_MAY produces false negatives on real struct mismatches**: 
  - In `page_in_sets(may=True)`, unioning (`inn[j] | out`) guarantees that if register `xN` holds a page on *any* converging path, it is treated as a page register. If Path A loads an adrp page into `x1` and Path B loads a heap/struct pointer into `x1`, instructions downstream of the join will strip the struct offset on Path B to `#LO]`.
  - Deliberately dismissing ADDR_MAY deltas of `8`, `16`, and `64` bytes in `rkisp_sd_s_rx_buffer`, `snd_card_init`, and `binder_alloc_deferred_release` as "global placement" ignores that `8`, `16`, and `64` are canonical struct alignment and field-shift deltas caused by mismatched `#ifdef`s or kernel config options.

- **Missing LSE atomics and LR tracking in register analysis**:
  - `page_in_sets` (lines 94–101) only discards `regs[0]` (and `regs[1]` for `ldp`). For ARMv8.1 LSE atomics (`ldadd`, `ldset`, `swp`, `cas`), the destination register being loaded into is `regs[1]` (or `regs[0]` and `regs[1]` for `casp`). An adrp base in `regs[1]` overwritten by an atomic load is not discarded.
  - Line 99 (`cur -= {'x%d' % r for r in range(19)}`) does not discard `x30` (`lr`). `bl`/`blr` overwrites `x30`. If an `adrp x30, ...` occurs before a call, `x30` remains in `cur` across the call boundary.

---

### IMPORTANT

- **`semtokens.py` multiset comparison is order-blind and branch-blind**:
  - `semtokens.py` compares token `Counter`s. It cannot detect:
    - Inverted conditional branch logic (`b.ne` vs `b.eq`). Branch condition mnemonics are ignored.
    - Swapped arguments (e.g., `foo(a, b)` vs `foo(b, a)` when both are immediate-derived).
    - Basic block migration: an immediate moved inside an `if` block vs outside an `if` block yields identical token multisets.
    - Missing bitfield instructions: `ubfx`, `sbfx`, `bfi`, `extr`, `bic`, `orn` are omitted from the regex check (line 25), silently dropping bitmask constants.
  - It runs `norm(..., flow_insensitive=True)` first, so any immediate already converted to `#LO` is invisible to `semtokens.py`.

- **CFG reconstruction drops branch targets**:
  - In `page_in_sets` (lines 75–81), the regex `tgt` specifically requires `<fname(+0x...)>`. If `llvm-objdump` targets a local symbol, an alternate alias, or displays an unadorned hex address, `t` resolves to `None`. 
  - For conditional branches (`b.cond`, `cbz`, `tbz`), an unresolved target results in dropping the taken branch entirely and retaining only the fallthrough edge (`i + 1`), corrupting the dataflow sets.

- **Unreachable code seeding pollutes convergence**:
  - Lines 86–90 search for `nxt = next((i for i in range(n) if inn[i] is None), None)` and seed `inn[nxt] = frozenset()`.
  - In ARM64 kernel binaries, switch jump tables or cold exception paths often follow an unconditional branch or `ret`. Seeding `inn[nxt]` and running worklist propagation allows disconnected tail code to propagate into subsequent blocks or loops, altering must-analysis sets.

- **Admitted logic rewrite in "equivalent" functions**:
  - In `RECON_NOTES.md`, Class 2 lists `rk817_battery_work` as having "range checks rewritten". A rewritten range check is an empirical logic difference, not an artifact or equivalence.
  - Calling joypad math functions (`joypad2touchpanel_abs_z_mode2`, etc.) "equivalent after path-by-path review" based on decompilation pseudocode without exact instruction equivalence or bit-exact unit test vectors is insufficient for stock fidelity.

- **Kernel configuration divergence disguised as "global placement"**:
  - Class 3 includes scheduler functions (`pick_next_task_fair`, `scheduler_tick`) and memory management (`free_large_kmalloc`). Core kernel `.bss`/`.data` displacement indicates mismatched `Kconfig` options, differences in struct sizes included prior to those globals, or out-of-tree patches.

---

### MINOR

- **Veneer and PLT boundary handling**:
  - Linker branch veneers (e.g. `__CortexA53843419_*`) alter intra-function branch targets and insert intermediate jump instructions that break size and instruction alignment checks between stock and reconstructed builds.
- **`sym` substitution regex greediness**:
  - Line 128 (`SYM = re.compile(r'0x[0-9a-f]+ <([^>+]+)(\+0x[0-9a-f]+)?>')`) relies on exact symbol names parsed by objdump. If stock ELF stripped local symbols while your vmlinux retained them, `sym` maps one side to `<func+off>` and the other to `<.Llocal>`, generating diffs that are pure disassembly artifacts.

---

### Alternative & Tradeoffs

- **Alternative**: Replace ad-hoc regex normalization with a symbol-aware relocation subtraction against unstripped ELF relocations, or use a DWARF/type-driven binary lifter (such as BAP or Ghidra P-Code IR) that computes actual symbolic expressions `[base + offset]` relative to resolved symbol definitions.
- **Tradeoff**: Relocation-based comparison requires exact linker script alignment and build artifact preservation, but eliminates the entire class of false matches where different struct fields/offsets are collapsed into `#LO]`.

---

### Confidence Verdict

**Claim rejected.** Confidence must remain **Moderate** (or drop to **Low-to-Moderate**). The claim of 69,170 `STRICT_SAME` functions is mathematically unsound because `strict_audit.py` systematically collapses distinct struct offsets, table indices, and base-relative constants to `#LO]`, actively masking code divergence.

#### Mandatory Checks Before "High Confidence":
1. **Remove blanket `#LO` masking**: Only normalize `#LO` if the target symbol can be statically resolved to the exact same variable/symbol in both binaries and the offset matches the symbol's offset within the page.
2. **Eliminate multiset equivalence**: Replace `semtokens.py` with basic-block control flow graph isomorphism verification (e.g., normalized P-code or LLVM IR graph diffing).
3. **Formal verification for Class 2**: For rewritten driver routines (`rk817_battery_work`, joypad functions), prove semantic equivalence using an SMT solver (e.g., Z3) on instruction semantics over the full 32/64-bit input domain, or match the assembly instruction-for-instruction.
4. **Explain Class 3 / ADDR_MAY root causes**: Prove why core scheduler globals shifted. Until `.config` parity produces zero global ordering displacement in core subsystems (`kernel/sched/`, `mm/`), the toolchain and configuration parity is incomplete.
