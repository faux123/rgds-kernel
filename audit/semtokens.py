#!/usr/bin/env python3
"""semtokens.py <our vmlinux> <strict.tsv>: for each non-ADDR_FLOW row, compare multisets of
semantic tokens (immediates in mov/cmp/arith, non-address memory offsets, call targets).
Equal multisets -> instruction order/register choice only; differing tokens are printed."""
import sys, re, collections, subprocess
sys.path.insert(0, '/extra2/projects/home/rgds-kernel/audit')
import strict_audit as S
ours, tsv = sys.argv[1], sys.argv[2]
names = [l.split('\t')[1] for l in open(tsv).read().splitlines()[1:] if not l.startswith('ADDR_FLOW')]
sr, orr = S.text_ranges(S.STOCK), S.text_ranges(ours)
def toks(elf, rng, fn):
    a, b = rng[fn]
    out = subprocess.run([S.T + 'llvm-objdump', '-d', '--no-show-raw-insn', '--start-address=%#x' % a, '--stop-address=%#x' % b, elf], capture_output=True, text=True).stdout
    seq, addrs = [], []
    for l in out.splitlines():
        m = re.match(r'^\s*([0-9a-f]{16}):\s+(\S+)\s*(.*)$', l)
        if m: addrs.append(int(m.group(1), 16)); seq.append((m.group(2), m.group(3).strip()))
    n = S.norm(fn, seq, a, b, addrs, True)
    c = collections.Counter()
    for op, arg in n:
        if op in ('bl',) or (op == 'b' and '<SELF' not in arg): c[('call', arg)] += 1
        for imm in re.findall(r'#(-?0x[0-9a-f]+|-?\d+)', arg):
            if op in S.LDST:
                if re.search(r'\[(sp|x29)\b', arg) or re.search(r'\[x18\]', arg):
                    continue  # stack frame and shadow-call-stack slots: register allocation, not source
                c[('off', op[:3], int(imm, 0))] += 1
            elif op in ('sub', 'add') and re.match(r'\s*sp\b', arg):
                continue
            elif op in ('mov', 'movk', 'cmp', 'cmn', 'and', 'orr', 'eor', 'tst', 'sub', 'add', 'ccmp', 'tbz', 'tbnz', 'lsl', 'lsr', 'asr', 'mul'):
                c[('imm', int(imm, 0))] += 1
    return c
for fn in names:
    if fn not in sr or fn not in orr: continue
    try: s, o = toks(S.STOCK, sr, fn), toks(ours, orr, fn)
    except Exception as e: print(fn, 'ERR', e); continue
    if s == o: print('%-36s TOKENS_EQUAL' % fn); continue
    print('%-36s stock-only %s | ours-only %s' % (fn, dict(s - o), dict(o - s)))
