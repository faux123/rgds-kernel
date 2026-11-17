### CRITICAL

- **Bijection Check is Circumvented by Fallback Sets (`strict2.py:157–172`)**:
  - In `norm()`, when `abstract=False`, instructions are compared literally. But `adrp` is *always* unconditionally collapsed: `out.append((op, S.xreg(...) + ', <P>'))` (lines 142–143).
  - If a function accesses the same page offset on a completely different page (e.g. Stock: `adrp x0, pageA; ldr x1, [x0, #0x10]`; Ours: `adrp x0, pageB; ldr x1, [x0, #0x10]`), `norm(..., abstract=False)` yields identical text.
  - The function is classified as `EXACT` (line 198) despite loading from completely different variables.
  - Line 213–216 records `EXACT` pairs into `fwd`/`rev`, but because it was declared `EXACT`, it **bypasses the `ADDR_CONF` rejection logic** (line 224: `for n, prs in pairs_by_fn.items()`, but `pairs_by_fn` is *only* populated for `ADDR` on line 203). An invalid mapping created in an `EXACT` function will silently corrupt downstream `ADDR_MAPPED` queries without ever failing the check.

- **`anchored()` Interpolation Masks Data Layout Collisions (`strict2.py:118–132`)**:
  - `anchored()` accepts unproven addresses if the surrounding proven keys within a 4 KiB span share the same delta ($b - a$).
  - In ARM64 Linux, if a subsystem has multiple global variables or struct instances grouped within a 4 KiB page, and one struct has an inserted or removed 8-byte field, any internal accesses that shift by that same delta will be validated as "anchored" even though the offset within the struct/table is fundamentally incorrect.
  - A 4 KiB window is the size of an entire MMU page. Treating the interior of any 4 KiB range as rigidly contiguous when only endpoints are mapped assumes no internal padding or struct field shifts occurred between those endpoints.

- **`rodata_check.py` Silently Skips Shifted Words and Type Differences (`rodata_check.py:91–98`)**:
  - Line 91: `if x % 8 != y % 8: continue`. If a struct field or global variable shifted alignment (e.g., from an unaligned offset or 4-byte member reordering), `rodata_check.py` **skips verification entirely**.
  - Line 93: `room(y)` restricts checking strictly to the size of the object in *our* build (`SZK`), completely ignoring the size of the object in stock. If our object was compiled smaller than stock's (e.g., missing struct array entries), bytes beyond `room(y)` are never read, hiding truncated tables.
  - Line 78: If `s and v - sy[0][i] > 4096`, it truncates to the first 24 bytes (`[:24]`). Large constant tables, lookup arrays, and crypto matrices are only checked across their first 3 words.

- **Broken Register Tracking for Indirect Calls and Partial Clobbers (`strict2.py:48–56`)**:
  - Line 48: `if ATOMIC.match(op): ... for r in re.findall(r'\b([xw]\d+)\b', arg): if S.xreg(r) != b: cur.pop(...)`. For store-exclusives (`stxr w0, x1, [x2]`), `w0` is the status result, `x1` is the source value, and `x2` is the base. `cur.pop()` evicts `x1` (the value being stored) from tracking even though `x1` is strictly read, not clobbered.
  - Line 56: For `ldst` pre/post-index, `cur.pop(S.base_reg(arg), None)`. But `ldp x0, x1, [x2], #16` clobbers `x0`, `x1`, AND modifies `x2`. Line 52 pops `regs[0]`, Line 54 pops `regs[1]`, Line 57 pops `base_reg`. But for standard `ldr x0, [x1, #8]!`, Line 52 matches `x0`, but `S.REG.match(regs[0])` matches `x0`, popping it, and line 57 pops `x1`. However, if the destination register is written via an unhandled ALU instruction (e.g., `bic`, `bfi`, `sbfx`, `extr`), `cur` is **not** invalidated, allowing dirty registers to masquerade as valid page bases.

---

### IMPORTANT

- **Jump-Label NOP Fallback Corrupts Dataflow State (`strict2.py:73–82`)**:
  - When unreachable code is encountered (`inn[nxt] is None`), the tool pulls `outs = [nop_out[j] for j in nop_out]` and intersects their state.
  - Static jump labels (`static_branch_unlikely`) in the Linux kernel do not follow uniform register allocation. One `nop` site may have `x19` holding `pageA`, while another unrelated `nop` site 500 instructions later has `x19` holding `pageB`.
  - Intersecting these dictionaries drops all mismatched registers. But if an unused register happens to hold the same page across two distant static keys by coincidence, that page fact is injected into the entry of disconnected cold code, fabricating false dataflow assertions.

- **Class 2 Logic Rewrites Invalidate Bit-Exact Claims (`RECON_NOTES.md`)**:
  - The author states `rk817_battery_work` "range checks rewritten" are mathematically equivalent (`x/10 == 0` vs `(unsigned)(x+9) <= 18`).
  - This confirms the source code is **divergent** from stock. The compiler does not synthesize division by 10 (`sdiv` / reciprocal multiplication) into an unsigned add-and-compare unless the source code itself was changed or compiled with fundamentally different optimization flags.
  - Manual review of decompilation is not an equivalence proof.

- **Core Scheduler & Memory Management Divergence Handwaved as "Global Placement"**:
  - `pick_next_task_fair`, `scheduler_tick`, `update_rq_clock`, `free_large_kmalloc` remain in `DIFF` (s2-r5.tsv) with reasons like `no page base w8, [x20, #0xa5b] | w8, [x20, #0x6db]`.
  - These are not linker shifts; the analysis failed to trace `x20` back to an `adrp`, which means `x20` is an incoming argument, loaded from a struct, or loaded via double indirection.
  - Offset `0xa5b` vs `0x6db` (delta of 896 bytes) inside `pick_next_task_fair` indicates that `struct rq`, `struct task_struct`, or `struct sched_entity` has altered field layouts caused by mismatched `Kconfig` options (e.g., `CONFIG_FAIR_GROUP_SCHED`, `CONFIG_UCLAMP_TASK`, or debug options).

---

### Mandatory Checks Audit

1. **Remove blanket `#LO` masking**: **Partly Met**. `strict2.py` replaced pure `#LO` string stripping with `fwd`/`rev` bijection tracking and `norm(abstract=False)`. However, `norm()` still strips `adrp` globally to `<P>` without comparing page addresses during `EXACT` matching, allowing different pages accessing identical low offsets to register as `EXACT` and escape conflict tracking.
2. **Eliminate multiset equivalence**: **Met**. `semtokens.py` has been eliminated from the primary classification pipeline in favor of `strict2.py` CFG dataflow tracking and literal opcode/operand matching.
3. **Formal verification for Class 2**: **Unmet**. Claude relied on inspection and manual arithmetic assertions (e.g., claiming `x/10 == 0` is equivalent to `(unsigned)(x+9) <= 18`). No SMT solver, translation validation tool, or compiler proof was supplied.
4. **Explain Class 3 / ADDR_MAY root causes**: **Unmet**. ADDR_MAY was re-badged as `ADDR_MAPPED`, but core scheduler functions (`pick_next_task_fair`, `update_rq_clock`, `scheduler_tick`) still fail to map cleanly and remain in `DIFF`. Displacements of several hundred bytes in `kernel/sched/` point directly to struct member layout divergence from mismatched `Kconfig` symbols.

---

### Verdict

**CONFIDENCE: REJECTED (Remains Moderate).**

While `strict2.py` is a substantial improvement over the previous linear script, it introduces masking pathways through unvalidated `EXACT` adrp collapsing and generous 4 KiB `anchored()` assumptions. Furthermore, core scheduler and memory management functions remain unresolved in the `DIFF` table, proving that struct layouts in foundational kernel subsystems do not match the stock binary. High confidence cannot be granted.
