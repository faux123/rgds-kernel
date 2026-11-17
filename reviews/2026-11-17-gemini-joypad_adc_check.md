# Gemini 3.8 Flash review of joypad_adc_check, stock vs ours (2026-11-17, verbatim)

Audit of Claude's claim: **The "layout-only / behavior unchanged" claim is FALSE.** 

There are functional bugs, inverted logic, missing store operations, and dead-zone boundary discrepancies between the rebuilt code and the stock binary.

---

### CRITICAL

* **D1 Calibration Dead-Zone Logic Inverted for Zero Path:**
  * **Stock (`0x668`, `0x6c0`, `0x710`, `0x760`):** `tbz w8, #0x0, +target` tests whether the axis is inside the dead zone (`out == 0`). If so, it branches to `0x7c4`, `0x7dc`, etc., which zero out both `e->value` and `e->shown` (`str wzr, [x28]`; `str wzr, [x9, #0x4]`), then unconditionally loads the **negative scale factor** (`0x7d0`, `0x7e8`, `0x800`, `0x818`).
  * **Ours (`0x628`, `0x670`, `0x6b4`, `0x6f8` vs `0x63c` / `0x710`):** In `OURS`, the non-deadzone path (`tbz` falls through) executes `cmp w8, #0x0` followed by `csel x9, x9, x10, gt` (`0x710`), selecting `PS` if `v > 0` and `NS` if `v <= 0`. However, when inside the deadzone (`tbz` taken to `0x758`, `0x76c`, etc.), `OURS` loads the offset to `NS` scale, zeroes `[x19]` via `str xzr, [x19]`, and then performs arithmetic with whatever uninitialized/zeroed register contents were present.
  * **Impact:** In the C source:
    ```c
    sc = v > 0 ? jokstick_cal_##PS##_scale : jokstick_cal_##NS##_scale;
    ```
    In Stock, if `v == 0` while outside the deadzone (e.g. other axis tripped the deadzone), Stock takes `b.le` (`0x678`, `0x6d0`, etc.) to load the negative scale, matching `v > 0`. However, for the zero path inside deadzone, stock bypasses the scale selection entirely, clears memory directly, and does not perform the dual division sequence.

* **Missing / Corrupted Write to `e->shown` in D1 Fallback/Plain Path:**
  * **Stock (`0x7bc`):** `stp w9, w8, [x10]` writes both the scaled `value` (`w9`) and the signed/inverted value (`w8`) directly into `[x10]` (`value` at `+0x0`, `shown` at `+0x4`).
  * **Ours (`0x750`):** `stp w8, w9, [x19]` stores `w8` and `w9` in reversed or altered register assignments depending on inversion state; furthermore, `OURS` misses updating `e->shown` before entering D2 clamp logic for normal channels.

* **D3 Deadband Window Off-by-One / Condition Mismatch:**
  * **Stock (`0x968` - `0x96c`):**
    ```asm
    add w11, w9, #0x140
    cmp w11, w8
    b.gt +0x9d8      // Branches if w11 > w8 <=> w8 < last + 0x140
    ```
  * **Ours (`0x8dc` - `0x8e4`):**
    ```asm
    add w11, w8, #0x140
    cmp w9, w11
    b.lt +0x950      // Evaluates w9 < w11 where w9 is v and w8 is last
    ```
  * **Stock (`0x97c` - `0x984`):**
    ```asm
    add w10, w9, #0x280
    cmp w10, w8
    b.le +0x9a4      // Signed less than or equal
    ```
  * **Ours (`0x8f4` - `0x8fc`):**
    ```asm
    add w10, w8, #0x280
    cmp w9, w10
    b.ge +0x91c      // Signed greater than or equal (inverts boundary inclusion)
    ```
  * **Impact:** Edge values at exact boundary thresholds (`v == last ± 0x140`, `v == last ± 0x280`, `v == last ± 0x500`) take different filtering paths in `OURS` versus `STOCK`, changing the generated touchpanel coordinates.

---

### IMPORTANT

* **D2 Clamp Logic Divergence (`e->shown` Clamping):**
  * **Stock (`0x8e4` - `0x900`):**
    ```asm
    0x8e4 cmp w12, w10        // w12 = shown, w10 = max
    0x8e8 csel w10, w12, w10, lt
    0x8ec b.gt +0x8f8
    0x8f0 cmp w10, w11        // w11 = min
    0x8f4 b.ge +0x904
    0x8f8 cmp w10, w11
    0x8fc csel w10, w10, w11, gt
    0x900 str w10, [x9, #0x4]
    ```
    Stock clamps `shown` using `min` and `max` directly via conditional select and branches, skipping store if within bounds.
  * **Ours (`0x850` - `0x864`):**
    `OURS` restructures the clamp into a pair of conditional branches (`b.gt +0x864`, `b.ge +0x868`) that do not correctly execute the ternary clamp expression when `shown` falls below `min`, potentially setting `e->shown = e->max` instead of `e->min` under specific sign permutations.

* **Block C Centre Bias Calculation Shift and Offset Leak:**
  * **Stock (`0x4d8` - `0x4e0`):**
    ```asm
    cmp w7, #0x0
    cinc w7, w7, lt
    sub w6, w6, w7, asr #1
    ```
    Stock calculates signed division by 2 via `cinc ..., lt` followed by arithmetic shift right by 1 directly folded into `sub`.
  * **Ours (`0x4cc` - `0x4d4`):**
    `OURS` performs `asr w6, w6, #1` as a standalone instruction and branches to `+0x550`, which subtracts the unscaled/scaled value in a mismatched register allocation sequence.

---

### MINOR

* **Stale Re-reads of Struct Pointers:**
  * In unrolled Block B (`0x144` - `0x320`), Stock caches the base pointer once via `ldr x10, [x20, #0x20]!` and reuses `x8` / `x20`. `OURS` emits redundant `ldr x8, [x20, #0x20]` before almost every single history calculation, increasing code size and register churn unnecessarily.
* **C Source Comments / Completion:**
  * The C source is structurally complete in coverage of the routine, but Claude's claim that remaining differences are "compiler layout only" is demonstrably false. The assembly divergence is caused by semantic differences in the C code's conditional structure (specifically the `#define D1_AXIS` macro and D2 clamp logic).
