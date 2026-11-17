#!/usr/bin/env python3
"""sdsum.py sd-ab.txt: median and min..max per (kind, scheduler, read-ahead) for each SD metric."""
import re
import statistics
import sys
from collections import defaultdict

runs = defaultdict(list)
cur = None
for line in open(sys.argv[1]):
    m = re.match(r'arm round=(\d+) sched=(\S+) ra=(\d+) kind=(\w+)', line)
    if m:
        cur = {}
        runs[(m.group(4), m.group(2), int(m.group(3)))].append(cur)
        continue
    m = re.match(r'\s+(\w+)=([\d.]+)$', line)
    if m and cur is not None:
        cur[m.group(1)] = float(m.group(2))

COLS = {'idle': [('seq_mbps', 'ROM load MB/s'), ('seq_s', 'ROM load s (256 MiB)'),
                 ('rand_mbps', '32 KiB random MB/s'), ('rand_p99_ms', '32 KiB random p99 ms'),
                 ('scan_s', 'scan 300 ROM headers s'), ('scan_p99_ms', 'scan p99 ms/file')],
        'write': [('rand_mbps', 'random MB/s during write'), ('rand_p50_ms', 'p50 ms'),
                  ('rand_p99_ms', 'p99 ms'), ('rand_max_ms', 'max ms')]}

for kind, cols in COLS.items():
    keys = sorted(k for k in runs if k[0] == kind)
    if not keys:
        continue
    print(f'\n### {kind}')
    print('| scheduler | read-ahead KiB | n | ' + ' | '.join(c[1] for c in cols) + ' |')
    print('|---|---|---|' + '---|' * len(cols))
    for k in keys:
        cells = []
        for c, _ in cols:
            v = [r[c] for r in runs[k] if c in r]
            cells.append(f'{statistics.median(v):.2f} ({min(v):.2f}..{max(v):.2f})' if v else '-')
        print(f'| {k[1]} | {k[2]} | {len(runs[k])} | ' + ' | '.join(cells) + ' |')
