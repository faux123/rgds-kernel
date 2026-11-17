
### 0 background hogs (n = 8 stock CFS arms, 8 BORE arms)
| Metric | Stock CFS median [IQR] | BORE median [IQR] | Change | IQRs overlap |
|---|---|---|---|---|
| wakeup latency p50 (us) | 61.7 [61.7..61.8] | 61.8 [61.7..61.8] | +0.1% | yes |
| wakeup latency p99 (us) | 83.0 [76.0..104.4] | 79.1 [76.2..86.8] | -4.8% | yes |
| wakeup latency p99.9 (us) | 173.2 [148.3..237.0] | 168.0 [128.8..267.8] | -3.0% | yes |
| wakeup latency max (us) | 517.9 [277.1..3529.1] | 564.7 [381.0..1428.7] | +9.0% | yes |
| 60 Hz frame: time to get 6 ms CPU, p50 (us) | 6000.2 [6000.2..6000.2] | 6000.2 [6000.2..6000.2] | +0.0% | yes |
| 60 Hz frame: time to get 6 ms CPU, p99 (us) | 6004.4 [6000.5..6126.5] | 6000.5 [6000.5..6002.2] | -0.1% | yes |
| 60 Hz frames missed (%) | 0.0 [0.0..0.0] | 0.0 [0.0..0.0] |  | yes |
| 100 fork+exec (ms) | 2513.2 [2502.6..2880.3] | 2508.2 [2498.8..2530.3] | -0.2% | yes |
| SoC temperature at arm start (C) | 55.2 [53.8..59.4] | 55.6 [52.5..58.3] | +0.6% | yes |

### 4 background hogs (n = 8 stock CFS arms, 8 BORE arms)
| Metric | Stock CFS median [IQR] | BORE median [IQR] | Change | IQRs overlap |
|---|---|---|---|---|
| wakeup latency p50 (us) | 61.0 [60.2..64.2] | 60.5 [60.3..61.9] | -0.7% | yes |
| wakeup latency p99 (us) | 232.4 [69.7..4535.2] | 98.3 [89.9..327.9] | -57.7% | yes |
| wakeup latency p99.9 (us) | 1510.0 [564.5..7919.3] | 1025.4 [652.5..3501.3] | -32.1% | yes |
| wakeup latency max (us) | 2418.1 [1502.5..8847.7] | 2775.2 [1951.1..5276.7] | +14.8% | yes |
| 60 Hz frame: time to get 6 ms CPU, p50 (us) | 6000.2 [6000.2..6000.2] | 6000.2 [6000.2..6000.2] | +0.0% | yes |
| 60 Hz frame: time to get 6 ms CPU, p99 (us) | 6000.8 [6000.5..6003.4] | 6000.5 [6000.5..6022.9] | -0.0% | yes |
| 60 Hz frames missed (%) | 0.0 [0.0..0.0] | 0.0 [0.0..0.0] |  | yes |
| 100 fork+exec (ms) | 2882.1 [2834.3..3777.8] | 2758.4 [2684.3..2819.3] | -4.3% | no |
| background throughput during latency test (kiter/s) | 1083.5 [1039.0..1098.3] | 1088.8 [1083.8..1096.4] | +0.5% | yes |
| background throughput during frame test (kiter/s) | 972.9 [946.1..994.0] | 999.0 [997.7..1001.7] | +2.7% | no |
| background throughput during fork test (kiter/s) | 869.0 [820.2..897.6] | 868.4 [865.0..876.9] | -0.1% | yes |
| SoC temperature at arm start (C) | 53.1 [51.2..57.2] | 52.5 [50.0..56.1] | -1.2% | yes |

### 16 background hogs (n = 8 stock CFS arms, 8 BORE arms)
| Metric | Stock CFS median [IQR] | BORE median [IQR] | Change | IQRs overlap |
|---|---|---|---|---|
| wakeup latency p50 (us) | 60.7 [60.5..62.1] | 60.8 [60.8..62.2] | +0.2% | yes |
| wakeup latency p99 (us) | 113.3 [85.0..492.8] | 96.0 [89.7..225.2] | -15.3% | yes |
| wakeup latency p99.9 (us) | 1836.1 [423.6..3529.4] | 1128.0 [856.5..3275.0] | -38.6% | yes |
| wakeup latency max (us) | 3258.8 [1004.5..5543.5] | 2451.5 [1637.3..7181.3] | -24.8% | yes |
| 60 Hz frame: time to get 6 ms CPU, p50 (us) | 12129.7 [11997.7..12342.8] | 6000.2 [6000.2..6000.2] | -50.5% | no |
| 60 Hz frame: time to get 6 ms CPU, p99 (us) | 25650.7 [24894.6..29754.1] | 6073.4 [6000.5..6433.3] | -76.3% | no |
| 60 Hz frames missed (%) | 35.2 [33.5..38.8] | 0.0 [0.0..0.0] | -100.0% | no |
| 100 fork+exec (ms) | 11086.2 [10885.0..11634.1] | 3789.9 [3756.5..4348.5] | -65.8% | no |
| background throughput during latency test (kiter/s) | 1088.5 [1084.6..1093.0] | 1088.2 [1083.8..1092.0] | -0.0% | yes |
| background throughput during frame test (kiter/s) | 1037.2 [1017.8..1042.6] | 992.8 [952.0..1001.7] | -4.3% | no |
| background throughput during fork test (kiter/s) | 996.2 [980.7..997.9] | 904.6 [899.1..911.2] | -9.2% | no |
| SoC temperature at arm start (C) | 56.9 [56.1..59.4] | 57.8 [56.7..59.4] | +1.5% | yes |
