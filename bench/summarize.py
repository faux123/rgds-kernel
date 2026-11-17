#!/usr/bin/env python3
"""summarize.py ab.txt: per load and metric, median [IQR] for bore=0 and bore=1, the change, and
whether the arms' interquartile ranges overlap (overlap = treat the change as noise), and the
full min..max range per arm so an outlier run is visible."""
import re
import statistics
import sys
from collections import defaultdict

runs = defaultdict(list)  # (load, bore) -> [dict]
cur = None
probe = None  # which probe printed the last metric: lat, frame or fork
for line in open(sys.argv[1]):
    m = re.match(r'arm round=(\d+) load=(\d+) bore=(\d) temp=(\d+)', line)
    if m:
        cur = {'temp_c': int(m.group(4)) / 1000}
        probe = None
        runs[(int(m.group(2)), int(m.group(3)))].append(cur)
        continue
    m = re.match(r'\s+(\w+)=([\d.]+)$', line)
    if m and cur is not None and m.group(1) not in ('done',):
        k = m.group(1)
        if k.split('_')[0] in ('lat', 'frame', 'fork'):
            probe = k.split('_')[0]
        # each probe prints its own hog throughput right after its metrics
        if k == 'hog_kiters_per_s':
            if probe is None:
                continue
            k = 'hog_kiters_per_s_' + probe
        cur[k] = float(m.group(2))

SHOW = [('lat_p50_us', 'wakeup latency p50 (us)', 1), ('lat_p99_us', 'wakeup latency p99 (us)', 1),
        ('lat_p999_us', 'wakeup latency p99.9 (us)', 1), ('lat_max_us', 'wakeup latency max (us)', 1),
        ('frame_p50_us', '60 Hz frame: time to get 6 ms CPU, p50 (us)', 1),
        ('frame_p99_us', '60 Hz frame: time to get 6 ms CPU, p99 (us)', 1),
        ('frame_missed_pct', '60 Hz frames missed (%)', 1), ('fork_total_ms', '100 fork+exec (ms)', 1),
        ('hog_kiters_per_s_lat', 'background throughput during latency test (kiter/s)', 0),
        ('hog_kiters_per_s_frame', 'background throughput during frame test (kiter/s)', 0),
        ('hog_kiters_per_s_fork', 'background throughput during fork test (kiter/s)', 0),
        ('temp_c', 'SoC temperature at arm start (C)', 1)]

def q(v):
    """median and interquartile range (linear interpolation, numpy/R type 7)."""
    v = sorted(v)
    if len(v) < 2:
        return v[0], v[0], v[0]
    q1, _, q3 = statistics.quantiles(v, n=4, method='inclusive')
    return statistics.median(v), q1, q3

for load in sorted({ld for ld, _ in runs}):
    a, b = runs[(load, 0)], runs[(load, 1)]
    print(f'\n### {load} background hogs (n = {len(a)} stock CFS arms, {len(b)} BORE arms)')
    print('| Metric | Stock CFS median [IQR] (min..max) | BORE median [IQR] (min..max) | Change | IQRs overlap |')
    print('|---|---|---|---|---|')
    for k, label, _ in SHOW:
        va = [r[k] for r in a if k in r]
        vb = [r[k] for r in b if k in r]
        if not va or not vb or (load == 0 and k.startswith('hog')):
            continue
        ma, la, ha = q(va)
        mb, lb, hb = q(vb)
        ch = 'n/a (base 0)' if ma == 0 else f'{(mb - ma) / ma * 100:+.1f}%'
        ov = 'yes' if not (hb < la or lb > ha) else 'no'
        print(f'| {label} | {ma:.1f} [{la:.1f}..{ha:.1f}] ({min(va):.1f}..{max(va):.1f}) '
              f'| {mb:.1f} [{lb:.1f}..{hb:.1f}] ({min(vb):.1f}..{max(vb):.1f}) | {ch} | {ov} |')
