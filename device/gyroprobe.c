/* gyroprobe: enable the Rockchip gyro through /dev/gyrosensor (L3G4200D_IOCTL_SET_ENABLE),
 * print the first 40 ABS events from the given event node, then disable it again.
 * Freestanding (raw syscalls), so it needs no Android sysroot. */
typedef unsigned long u64; typedef long s64;
static s64 sc(long n, long a, long b, long c, long d) {
    register long x8 asm("x8") = n; register long x0 asm("x0") = a; register long x1 asm("x1") = b;
    register long x2 asm("x2") = c; register long x3 asm("x3") = d;
    asm volatile("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}
static void out(const char *s) { long n = 0; while (s[n]) n++; sc(64, 1, (long)s, n, 0); }
static void num(long v) { char b[24]; int i = 23; b[i] = 0; int neg = v < 0; if (neg) v = -v;
    do { b[--i] = '0' + v % 10; v /= 10; } while (v); if (neg) b[--i] = '-'; out(b + i); }
void _start(void) {
    long *sp; asm("mov %0, sp" : "=r"(sp));
    char **argv = (char **)(sp + 1);
    int on = 1, off = 0;
    long g = sc(56, -100, (long)"/dev/gyrosensor", 2, 0);
    out("gyrosensor fd "); num(g); out("\n");
    out("enable ret "); num(sc(29, g, 0x40044d02, (long)&on, 0)); out("\n");
    long e = sc(56, -100, (long)argv[1], 0, 0);
    struct { long sec, nsec; } ts = { 0, 300000000 };
    int abs[6];
    for (int r = 0; r < 10; r++) {
        sc(101, (long)&ts, 0, 0, 0);
        out("sample");
        for (int a = 3; a <= 5; a++) {            /* ABS_RX, ABS_RY, ABS_RZ via EVIOCGABS */
            sc(29, e, 0x80184500 | (0x40 + a), (long)abs, 0);
            out(" "); num(abs[0]);
        }
        out("\n");
    }
    sc(29, g, 0x40044d02, (long)&off, 0);
    sc(93, 0, 0, 0, 0);
}
