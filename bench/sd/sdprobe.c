/*
 * sdprobe: SD card read patterns of a DS emulator setup, for the read-ahead / IO scheduler A/B.
 *
 *   sdprobe seq  <file>                 read the whole file in 128 KiB read()s (ROM load)
 *   sdprobe rand <file> <n> <kib> <seed> n random preads of <kib> KiB (streaming from a ROM)
 *   sdprobe scan <dir> <n> <kib>        open the first n files in <dir> (sorted) and read <kib> KiB
 *                                       from each (a frontend scanning ROM headers and icons)
 *
 * Callers drop the page cache before each run. Output: key=value lines.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static uint64_t now_ns(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000000000ull + t.tv_nsec;
}

static int cmp(const void *a, const void *b) {
	uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
	return x < y ? -1 : x > y;
}

static void lat(const char *name, uint64_t *v, size_t n) {
	qsort(v, n, sizeof(*v), cmp);
	printf("%s_p50_ms=%.3f\n%s_p99_ms=%.3f\n%s_max_ms=%.3f\n", name, v[n / 2] / 1e6, name,
	       v[n * 99 / 100] / 1e6, name, v[n - 1] / 1e6);
}

static int cmpstr(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int main(int argc, char **argv) {
	static char buf[1 << 20];
	if (argc < 3) { fprintf(stderr, "usage: see source\n"); return 2; }
	if (!strcmp(argv[1], "seq")) {
		int fd = open(argv[2], O_RDONLY);
		if (fd < 0) { perror("open"); return 1; }
		uint64_t s = now_ns(), tot = 0;
		ssize_t r;
		while ((r = read(fd, buf, 128 << 10)) > 0) tot += r;
		double sec = (now_ns() - s) / 1e9;
		printf("seq_mb=%.1f\nseq_s=%.3f\nseq_mbps=%.1f\n", tot / 1048576.0, sec, tot / 1048576.0 / sec);
		close(fd);
	} else if (!strcmp(argv[1], "rand") && argc >= 6) {
		int fd = open(argv[2], O_RDONLY), n = atoi(argv[3]), kib = atoi(argv[4]);
		struct stat st;
		if (fd < 0 || fstat(fd, &st) || n <= 0 || kib <= 0 || kib > 1024) { fprintf(stderr, "bad args\n"); return 2; }
		srand(atoi(argv[5]));
		uint64_t *v = malloc(n * sizeof(*v)), blocks = st.st_size / (kib << 10), s = now_ns();
		if (!v || !blocks) return 1;
		for (int i = 0; i < n; i++) {
			off_t off = (off_t)((((uint64_t)rand() << 16) ^ rand()) % blocks) * (kib << 10);
			uint64_t t = now_ns();
			if (pread(fd, buf, kib << 10, off) < 0) { perror("pread"); return 1; }
			v[i] = now_ns() - t;
		}
		double sec = (now_ns() - s) / 1e9;
		printf("rand_mbps=%.2f\n", (double)n * kib / 1024 / sec);
		lat("rand", v, n);
		close(fd);
	} else if (!strcmp(argv[1], "scan") && argc >= 5) {
		int n = atoi(argv[3]), kib = atoi(argv[4]), cnt = 0, cap = 4096;
		char **names = malloc(cap * sizeof(*names));
		DIR *d = opendir(argv[2]);
		struct dirent *e;
		if (!d || !names || n <= 0 || kib <= 0 || kib > 1024) { fprintf(stderr, "bad args\n"); return 2; }
		while ((e = readdir(d)) && cnt < cap)
			if (e->d_name[0] != '.') names[cnt++] = strdup(e->d_name);
		closedir(d);
		qsort(names, cnt, sizeof(*names), cmpstr);
		if (n > cnt) n = cnt;
		uint64_t *v = malloc(n * sizeof(*v)), s = now_ns();
		if (!v) return 1;
		for (int i = 0; i < n; i++) {
			char p[4096];
			snprintf(p, sizeof(p), "%s/%s", argv[2], names[i]);
			uint64_t t = now_ns();
			int fd = open(p, O_RDONLY);
			if (fd < 0) { perror(p); return 1; }
			if (read(fd, buf, kib << 10) < 0) { perror("read"); return 1; }
			close(fd);
			v[i] = now_ns() - t;
		}
		printf("scan_files=%d\nscan_s=%.3f\n", n, (now_ns() - s) / 1e9);
		lat("scan", v, n);
	} else { fprintf(stderr, "bad mode\n"); return 2; }
	return 0;
}
