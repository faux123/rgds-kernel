/*
 * bore-bench: scheduler latency/throughput probe for the RG DS BORE A/B.
 *
 *   bore-bench lat   <hogs> <seconds>   wakeup lateness of a 1 ms periodic SCHED_OTHER thread
 *   bore-bench frame <hogs> <seconds>   60 Hz "game thread": 6 ms of work per 16.667 ms frame
 *   bore-bench fork  <hogs> <count>     fork+exec /system/bin/true, total wall time
 *
 * Hogs are nice-0 busy threads (one per CPU by default) that also count loop iterations, so each
 * mode reports the background throughput it cost. All times are CLOCK_MONOTONIC. Output is one
 * key=value per line for the driver script.
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static atomic_int stop;
static uint64_t hog_iters[64];

static uint64_t now_ns(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000000000ull + t.tv_nsec;
}

static void *hog(void *arg) {
	uint64_t *ctr = arg, n = 0;
	while (!atomic_load_explicit(&stop, memory_order_relaxed)) {
		for (volatile int i = 0; i < 1000; i++) ;
		n++;
	}
	*ctr = n;
	return NULL;
}

static void spin_ns(uint64_t ns) {
	uint64_t end = now_ns() + ns;
	while (now_ns() < end) ;
}

static int cmp(const void *a, const void *b) {
	uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
	return x < y ? -1 : x > y;
}

static void pct(const char *name, uint64_t *v, size_t n) {
	qsort(v, n, sizeof(*v), cmp);
	printf("%s_n=%zu\n", name, n);
	printf("%s_p50_us=%.1f\n", name, v[n / 2] / 1e3);
	printf("%s_p90_us=%.1f\n", name, v[n * 9 / 10] / 1e3);
	printf("%s_p99_us=%.1f\n", name, v[n * 99 / 100] / 1e3);
	printf("%s_p999_us=%.1f\n", name, v[n * 999 / 1000] / 1e3);
	printf("%s_max_us=%.1f\n", name, v[n - 1] / 1e3);
}

int main(int argc, char **argv) {
	if (argc < 4) { fprintf(stderr, "usage: %s lat|frame|fork <hogs> <seconds|count>\n", argv[0]); return 2; }
	const char *mode = argv[1];
	int hogs = atoi(argv[2]), arg = atoi(argv[3]);
	pthread_t th[64];
	if (hogs < 0) hogs = 0;
	if (hogs > 64) hogs = 64;
	if (arg <= 0) { fprintf(stderr, "seconds/count must be > 0\n"); return 2; }
	for (int i = 0; i < hogs; i++) pthread_create(&th[i], NULL, hog, &hog_iters[i]);
	usleep(200000); /* let hogs build burst history */
	uint64_t t0 = now_ns();

	if (!strcmp(mode, "lat")) {
		size_t cap = (size_t)arg * 1000 + 16, n = 0;
		uint64_t *v = malloc(cap * sizeof(*v));
		if (!v) { perror("malloc"); return 1; }
		struct timespec next;
		clock_gettime(CLOCK_MONOTONIC, &next);
		uint64_t end = now_ns() + (uint64_t)arg * 1000000000ull;
		while (now_ns() < end && n < cap) {
			next.tv_nsec += 1000000;
			if (next.tv_nsec >= 1000000000) { next.tv_nsec -= 1000000000; next.tv_sec++; }
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
			uint64_t target = (uint64_t)next.tv_sec * 1000000000ull + next.tv_nsec, t = now_ns();
			v[n++] = t > target ? t - target : 0;
		}
		pct("lat", v, n);
	} else if (!strcmp(mode, "frame")) {
		const uint64_t period = 16666667, work = 6000000;
		size_t cap = (size_t)arg * 61 + 16, n = 0, missed = 0;
		uint64_t *v = malloc(cap * sizeof(*v));
		if (!v) { perror("malloc"); return 1; }
		uint64_t start = now_ns(), end = start + (uint64_t)arg * 1000000000ull;
		uint64_t k = 1; /* frame k must finish by start + k * period */
		while (now_ns() < end && n < cap) {
			uint64_t fs = now_ns();
			spin_ns(work);
			uint64_t fe = now_ns();
			v[n++] = fe - fs; /* wall time to get 6 ms of CPU */
			if (fe > start + k * period) missed++;
			k++;
			while (start + (k - 1) * period < now_ns()) k++; /* skip frames already past */
			uint64_t wake = start + (k - 1) * period;
			struct timespec ts = { (time_t)(wake / 1000000000ull), (long)(wake % 1000000000ull) };
			clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
		}
		pct("frame", v, n);
		printf("frame_missed=%zu\n", missed);
		printf("frame_missed_pct=%.2f\n", 100.0 * missed / n);
	} else if (!strcmp(mode, "fork")) {
		uint64_t s = now_ns();
		for (int i = 0; i < arg; i++) {
			pid_t p = fork();
			if (p == 0) { execl("/system/bin/true", "true", (char *)NULL); _exit(127); }
			int st; waitpid(p, &st, 0);
		}
		printf("fork_total_ms=%.1f\n", (now_ns() - s) / 1e6);
	} else { fprintf(stderr, "bad mode\n"); return 2; }

	double secs = (now_ns() - t0) / 1e9;
	atomic_store(&stop, 1);
	uint64_t tot = 0;
	for (int i = 0; i < hogs; i++) { pthread_join(th[i], NULL); tot += hog_iters[i]; }
	printf("hog_kiters_per_s=%.1f\n", hogs ? tot / secs / 1e3 : 0.0);
	printf("done=1\n");
	return 0;
}
