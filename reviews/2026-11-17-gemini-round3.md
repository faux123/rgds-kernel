### CRITICAL

- **`objcmp.py` Fails to Catch Divergence in Repeated Static Names (`objcmp.py:45–53`)**:
  - `cnt_s[n] == 1` and `cnt_o[n] == 1` unconditionally filter out any static symbol whose name appears more than once across the kernel (e.g. `driver_state`, `ops`, `capabilities`, `priv`, `table`, `dev_attr_*`).
  - Out of tens of thousands of objects in Linux, thousands of static device driver structs share names. **None of these are compared by `objcmp.py`.** A driver struct in a modified driver sharing a common identifier name with an upstream driver is silently dropped.

- **`objcmp.py` Silently Disables Oversized Stock Object Detection (`objcmp.py:60`)**:
  - Line 60: `if sgap < size or (sgap > size + 64 and sgap < 4096 and False): sizebad.append(...)`.
  - The condition `and False` hard-codes the check for `sgap > size + 64` to never execute.
  - If stock compiled a struct or table with extra trailing fields (e.g., Anbernic added vendor fields or array entries to an existing struct), stock's `sgap` will be larger than `size`. Because `size = O_all`'s size and `rs(x, size)` only reads `size` bytes from stock, any extra vendor trailing data in stock is never read, never compared, and never flagged as `sizebad`.

- **`.bss` Initial State and Anonymous Data Left Completely Unverified (`objcmp.py:12, 51`)**:
  - `LO, HI = 0xffffffc009570000, 0xffffffc00a404000` stops at `end of .data (not .bss)`. `.bss` objects are excluded by design from `objcmp.py`.
  - In `datanames.py:10`, all symbols starting with `.L` (compiler-generated anonymous constants, jump tables, compound literals, mergeable string pools) are discarded.
  - If the compiler inlined a compound literal or generated an anonymous lookup table into `.rodata`, neither `objcmp.py` (which keys on named symbols) nor `datanames.py` (which drops `.L`) verifies it unless it was accidentally hit by an `adrp` mapped instruction.

- **String Pool Collisions Create False Positives / False Equivalences (`objcmp.py:38–43`)**:
  - If an address falls into an object listed in `POOL` or `off > 65536`, `meaning()` slices the string at `\0` and takes `[:64]`.
  - If two pointers in stock and ours point to completely different error or format strings that happen to share the first 64 characters, they compare as equal.
  - Conversely, `task_index_to_char.state_char` in `POOL` is an array of chars (`RSDTtXZxKWn...`), not a null-terminated C-string. Slicing it with `.split(b'\0')[0][:64]` treats raw binary/char table data as null-terminated strings, causing truncation or spurious comparisons.

---

### IMPORTANT

- **Pointer Resolution Relies on Ambiguous NM Slices (`objcmp.py:38–43`, `datanames.py:14–16`)**:
  - `stock.elf` symbols are typed `t` (stripped text/data distinction from kallsyms or raw ELF reconstruction). When a pointer points to an interior offset of an anonymous or static section, `bisect_right` returns the nearest preceding symbol.
  - In `datanames.py`, the DIFF log proves this failure mode: functions are reporting name mismatches against unrelated distant symbols, e.g.:
    `cst8xx_touch_report`: `[('task_index_to_char.state_char', 226390)] | [('task_index_to_char.state_char', 226...)]`
    `goodix_ts_probe`: `[('linux_banner', 2888934)] | [('kallsyms_token_index', 41342)]`
  - When an address cannot be cleanly mapped to an isolated symbol, it falls back to offsets hundreds of kilobytes deep into `linux_banner` or `task_index_to_char`. This indicates data layout shifts where symbols are missing or completely misaligned, yet `datanames.py` simply records it as a name delta without rejecting the build.

- **Remaining DIFF List Contains Massive Divergence in Core Subsystems (`s2-r10`)**:
  - `cm_monitor_poller` has 132 unmatched data/instruction differences.
  - `sgm3784_set_ctrl` has 66 differences; `joypad2touchpanel_abs_ext` has 138 differences.
  - `joypad_adc_check` length is 682 vs 646 (36 instruction difference).
  - `joypad_probe` length is 985 vs 1028 (43 instruction difference).
  - `tramp_vectors` length is 544 vs 1568 (over 1 KiB difference in ARM64 exception trampolines).
  - `cma_activate_area`: `imm x10, [x20, #0x38] | x10, [x20, #0xf8]`. This is an offset difference of 192 bytes inside a struct access within core memory management. This is not register allocation or `__LINE__`; it is an unmistakable struct definition divergence (mismatched config options like `CONFIG_CMA_DEBUGFS` or `CONFIG_NUMA`).

- **Unmet Formal Equivalence on Logic Rewrites**:
  - The author still asserts `x/10 == 0` is equivalent to `(unsigned)(x+9) <= 18` for 32-bit signed ints without validation.
  - While both conditions are true for $-9 \le x \le 9$, the compiler emitting different code directly demonstrates that the source AST differs from stock. Any claim of bit-level source reconstruction is void for these functions.

---

### Audit of the 4 Mandatory Checks

1. **Remove blanket `#LO` masking**: **Partly Met**. Abstracted checks enforce bijection on page+LO when tracked, but unmapped or unanchored constants still bypass if collapsed to `<P>`, and anonymous literals falling back to string pool heuristics hide offsets.
2. **Eliminate multiset equivalence**: **Met**. CFG dataflow and literal matching are in place.
3. **Formal verification for Class 2**: **Unmet**. Reconstructed code in `rk817_battery_work` and joypad drivers contains structural C rewrites validated solely by inspection.
4. **Explain Class 3 / ADDR_MAY root causes**: **Unmet**. 73 functions remain in `DIFF`. The offsets in `cma_activate_area` (`#0x38` vs `#0xf8`), `read_cons_helper` (`#0x298` vs `#0x358`), and `aw36518_set_ctrl` prove struct member displacements that stem from differing `Kconfig` options or unapplied vendor patches.

---

### Residual Risk

The primary residual risk is **unmatched struct layouts in core MM and vendor drivers**. Displacements like 192 bytes in `cma_activate_area`, differing trampoline sizes in `tramp_vectors`, and dozens of length/instruction mismatches across `joypad_adc_check`, `joypad_probe`, and `hyn_probe` prove that:
1. Vendor patches modifying common data structures are missing from the reconstructed source.
2. Driver C code has been heuristically approximated to match behavior rather than reconstructed to match exact compilation output.
3. `objcmp.py` provides a false sense of security by dropping all non-unique symbol names and explicitly disabling upper-bound size checks (`and False`).

---

### Verdict

**CONFIDENCE: REJECTED.**

Confidence cannot be high. `objcmp.py` blinds itself to duplicate static symbols, ignores `.bss`, ignores trailing struct expansions via hardcoded `and False`, and truncates binary tables in `POOL`. Furthermore, 73 functions remain in `DIFF`, with undeniable structural differences (struct offsets shifting by hundreds of bytes in `cma_activate_area` and tens of instructions missing in driver probe functions). The source is not bit-equivalent to the stock kernel.
