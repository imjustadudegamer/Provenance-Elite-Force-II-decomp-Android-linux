#include "tr_texcache.h"
#include "tr_hwtex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define PROBE_MAX_FILES 4096

static const char *g_cacheBase = NULL;
static int         g_target = R201_TEXC_ETC2_RGBA8;
static int         g_picmip = 0;
static int         g_maxtex = 4096;
static int         g_mipmaps = 1;

static void host_join(char *out, int outSize, const char *qpath)
{
	snprintf(out, (size_t)outSize, "%s/%s", g_cacheBase, qpath);
}

static long host_open(void *ud, const char *qpath, r201TexCacheFile_t *fh)
{
	char path[1024];
	FILE *f;
	long len;

	(void)ud;
	host_join(path, sizeof(path), qpath);
	f = fopen(path, "rb");
	if (!f) return -1;
	if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
	len = ftell(f);
	if (len < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
	*fh = (r201TexCacheFile_t)f;
	return len;
}

static int host_read(void *ud, r201TexCacheFile_t fh, void *buf, int len)
{
	(void)ud;
	return (int)fread(buf, 1, (size_t)len, (FILE *)fh);
}

static void host_close(void *ud, r201TexCacheFile_t fh)
{
	(void)ud;
	fclose((FILE *)fh);
}

static int host_write(void *ud, const char *qpath, const void *buf, int len)
{
	char path[1024];
	FILE *f;
	size_t w;

	(void)ud;
	host_join(path, sizeof(path), qpath);
	f = fopen(path, "wb");
	if (!f) return 0;
	w = fwrite(buf, 1, (size_t)len, f);
	if (fclose(f) != 0) return 0;
	return (int)w == len;
}

static int host_remove(void *ud, const char *qpath)
{
	char path[1024];

	(void)ud;
	host_join(path, sizeof(path), qpath);
	remove(path);
	return access(path, F_OK) != 0;
}

static char **host_list(void *ud, int *count)
{
	char dir[1024];
	char **names = NULL;
	DIR *d;
	struct dirent *e;
	int n = 0, cap = 0;

	(void)ud;
	*count = 0;
	snprintf(dir, sizeof(dir), "%s/%s", g_cacheBase, R201_TEXCACHE_DIR);
	d = opendir(dir);
	if (!d) return NULL;
	while ((e = readdir(d)) != NULL) {
		size_t l = strlen(e->d_name);
		if (l < 5 || strcmp(e->d_name + l - 5, ".r2tc") != 0) continue;
		if (n == cap) {
			char **nn;
			cap = cap ? cap * 2 : 64;
			nn = (char **)realloc(names, sizeof(char *) * (size_t)cap);
			if (!nn) break;
			names = nn;
		}
		names[n] = strdup(e->d_name);
		if (!names[n]) break;
		n++;
	}
	closedir(d);
	*count = n;
	return names;
}

static void host_freelist(void *ud, char **names)
{
	int i;

	(void)ud;
	if (!names) return;
	for (i = 0; names[i]; i++) free(names[i]);
	free(names);
}

static const r201TexCacheIO_t g_io = {
	host_open, host_read, host_close, host_write, host_remove, host_list, host_freelist, NULL
};

typedef struct {
	int   width, height, numMips, dxt;
	long  storedBytes;
	const unsigned char *payload;
} probeDds_t;

static unsigned int rd32(const unsigned char *p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
		   ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static int probe_parse_dds(unsigned char *buf, long len, probeDds_t *d)
{
	if (len < 128 || memcmp(buf, "DDS ", 4) != 0) return 0;
	d->height = (int)rd32(buf + 12);
	d->width = (int)rd32(buf + 16);
	d->numMips = (int)rd32(buf + 28);
	if (d->numMips < 1) d->numMips = 1;
	if (memcmp(buf + 84, "DXT1", 4) == 0) d->dxt = 1;
	else if (memcmp(buf + 84, "DXT3", 4) == 0) d->dxt = 3;
	else if (memcmp(buf + 84, "DXT5", 4) == 0) d->dxt = 5;
	else return 0;
	if (d->width < 1 || d->height < 1) return 0;
	d->payload = buf + 128;
	d->storedBytes = R201_TexCache_StoredBytes(d->width, d->height, d->numMips, d->dxt);
	if (d->storedBytes > len - 128) {
		d->storedBytes = len - 128;
	}
	return 1;
}

static void probe_geometry(const probeDds_t *d, int *base, int *upW, int *upH, int *levels)
{
	int b = (g_picmip > 0) ? g_picmip : 0;

	if (d->numMips > 0 && b >= d->numMips) b = d->numMips - 1;
	if (b < 0) b = 0;
	while (b < 30 && ((d->width >> b) > g_maxtex || (d->height >> b) > g_maxtex)) b++;
	if (d->numMips > 0 && b >= d->numMips) b = d->numMips - 1;
	if (b < 0) b = 0;

	*base = b;
	*upW = d->width >> b;
	*upH = d->height >> b;
	if (*upW < 1) *upW = 1;
	if (*upH < 1) *upH = 1;

	*levels = 1;
	{
		int tw = *upW, th = *upH;
		while (g_mipmaps && (tw > 1 || th > 1)) {
			if (tw > 1) tw >>= 1;
			if (th > 1) th >>= 1;
			(*levels)++;
		}
	}
}

static double ms_since(clock_t a, clock_t b)
{
	return 1000.0 * (double)(b - a) / (double)CLOCKS_PER_SEC;
}

static void *probe_alloc(int bytes)
{
	return malloc((size_t)bytes);
}

static void probe_free(void *p)
{
	free(p);
}

static unsigned char *g_out;
static long           g_outLen, g_outCap;

static void out_bytes(const void *p, long n)
{
	if (g_outLen + n > g_outCap) {
		long cap = g_outCap ? g_outCap : (1 << 20);
		unsigned char *nn;
		while (cap < g_outLen + n) cap *= 2;
		nn = (unsigned char *)realloc(g_out, (size_t)cap);
		if (!nn) { fprintf(stderr, "out realloc failed\n"); exit(2); }
		g_out = nn;
		g_outCap = cap;
	}
	memcpy(g_out + g_outLen, p, (size_t)n);
	g_outLen += n;
}

static void out_u32(unsigned int v)
{
	unsigned char b[4];
	b[0] = (unsigned char)(v & 0xFF);
	b[1] = (unsigned char)((v >> 8) & 0xFF);
	b[2] = (unsigned char)((v >> 16) & 0xFF);
	b[3] = (unsigned char)((v >> 24) & 0xFF);
	out_bytes(b, 4);
}

typedef struct {
	int files, hits, misses, corrupt, disabled;
	int storeOk, storeFail, storeSkip, evicted;
	long long bytes;
	double hashMs, loadMs, buildMs, storeMs;
	int firstPathShown;
} probeStats_t;

static void probe_one(const char *path, probeStats_t *st)
{
	FILE *f = fopen(path, "rb");
	unsigned char *buf;
	long len;
	probeDds_t d;
	int base, upW, upH, levels, total;
	r201TexCacheKey_t key;
	r201TexCacheResult_t lres, sres;
	unsigned int lo, hi;
	int srcBytes;
	unsigned char *chain = NULL;
	unsigned char *cached = NULL;
	const char *baseName;
	clock_t t0, t1;
	int loadedBytes = 0;

	if (!f) { fprintf(stderr, "open %s failed\n", path); return; }
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = (unsigned char *)malloc((size_t)len);
	if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
		fprintf(stderr, "read %s failed\n", path);
		fclose(f);
		free(buf);
		return;
	}
	fclose(f);

	if (!probe_parse_dds(buf, len, &d)) {
		free(buf);
		return;
	}

	baseName = strrchr(path, '/');
	baseName = baseName ? baseName + 1 : path;

	probe_geometry(&d, &base, &upW, &upH, &levels);
	total = R201_TranscodeChainBytes(upW, upH, levels);

	t0 = clock();
	R201_TexCache_SourceHash(d.payload, d.width, d.height, d.numMips, d.dxt, &lo, &hi, &srcBytes);
	R201_TexCache_FillKey(&key, baseName, d.width, d.height, g_mipmaps, g_picmip,
						  g_maxtex, d.dxt, g_target, base, upW, upH, levels, lo, hi, srcBytes);
	t1 = clock();
	st->hashMs += ms_since(t0, t1);

	memset(&lres, 0, sizeof(lres));
	memset(&sres, 0, sizeof(sres));
	t0 = clock();
	if (R201_TexCache_Load(&key, &cached, &loadedBytes, probe_alloc, probe_free, &lres) &&
		cached && loadedBytes == total) {
		t1 = clock();
		st->loadMs += ms_since(t0, t1);
		st->hits++;
		chain = cached;
	} else {
		t1 = clock();
		st->loadMs += ms_since(t0, t1);
		if (cached) {
			free(cached);
			cached = NULL;
		}
		if (lres.stat == R201_TEXCACHE_STAT_CORRUPT) st->corrupt++;
		else if (lres.stat == R201_TEXCACHE_STAT_DISABLED) st->disabled++;
		else st->misses++;

		chain = (unsigned char *)malloc((size_t)total);
		if (!chain) { free(buf); return; }

		t0 = clock();
		R201_TranscodeChain(d.payload, d.dxt, d.width, d.height, d.numMips, base,
							upW, upH, levels, g_target, chain, probe_alloc, probe_free, NULL, NULL);
		t1 = clock();
		st->buildMs += ms_since(t0, t1);

		t0 = clock();
		R201_TexCache_Store(&key, chain, total, &sres);
		t1 = clock();
		st->storeMs += ms_since(t0, t1);
		if (sres.stat == R201_TEXCACHE_STAT_STORED) st->storeOk++;
		else if (sres.stat == R201_TEXCACHE_STAT_SKIP) st->storeSkip++;
		else st->storeFail++;
		st->evicted += sres.evicted + lres.evicted;
	}

	if (!st->firstPathShown) {
		printf("[R201:texcache-probe] path=%s name=%s target=%s base=%d levels=%d bytes=%d load=%s\n",
			   lres.path, baseName, (g_target == R201_TEXC_ASTC_4x4) ? "astc" : "etc2",
			   base, levels, total,
			   (lres.stat == R201_TEXCACHE_STAT_HIT) ? "hit" : "miss");
		st->firstPathShown = 1;
	}

	out_u32((unsigned int)strlen(baseName));
	out_bytes(baseName, (long)strlen(baseName));
	out_u32((unsigned int)total);
	out_bytes(chain, total);
	st->bytes += total;
	st->files++;
	free(chain);
	free(buf);
}

static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void probe_ensure_cache_dir(void)
{
	char p[1024];

	mkdir(g_cacheBase, 0777);
	snprintf(p, sizeof(p), "%s/%s", g_cacheBase, R201_TEXCACHE_DIR);
	mkdir(p, 0777);
}

static int collect_dir(const char *dir, char **files, int max)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	int n = 0;

	if (!d) return 0;
	while ((e = readdir(d)) != NULL) {
		size_t l = strlen(e->d_name);
		char *p;
		if (l < 5 || strcmp(e->d_name + l - 4, ".dds") != 0) continue;
		if (n >= max) break;
		p = (char *)malloc(strlen(dir) + l + 2);
		sprintf(p, "%s/%s", dir, e->d_name);
		files[n++] = p;
	}
	closedir(d);
	return n;
}

int main(int argc, char **argv)
{
	const char *dds = NULL, *ddsdir = NULL, *out = NULL;
	int maxmb = R201_TEXCACHE_DEFAULT_MAX_MB;
	char **files;
	int nfiles = 0, i;
	probeStats_t st;
	FILE *of;
	int single = 0;
	char singleBuf[1024];

	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--cache-base=", 13)) g_cacheBase = argv[i] + 13;
		else if (!strncmp(argv[i], "--dds=", 6)) { dds = argv[i] + 6; single = 1; }
		else if (!strncmp(argv[i], "--ddsdir=", 9)) ddsdir = argv[i] + 9;
		else if (!strncmp(argv[i], "--out=", 6)) out = argv[i] + 6;
		else if (!strncmp(argv[i], "--maxmb=", 8)) maxmb = atoi(argv[i] + 8);
		else if (!strncmp(argv[i], "--picmip=", 9)) g_picmip = atoi(argv[i] + 9);
		else if (!strncmp(argv[i], "--maxtex=", 9)) g_maxtex = atoi(argv[i] + 9);
		else if (!strncmp(argv[i], "--mipmaps=", 10)) g_mipmaps = atoi(argv[i] + 10);
		else if (!strncmp(argv[i], "--target=", 9)) {
			if (!strcmp(argv[i] + 9, "astc")) g_target = R201_TEXC_ASTC_4x4;
			else g_target = R201_TEXC_ETC2_RGBA8;
		}
	}
	if (!g_cacheBase || (!dds && !ddsdir)) {
		fprintf(stderr, "usage: %s --cache-base=DIR (--dds=FILE|--ddsdir=DIR) [--target=etc2|astc] [--out=FILE]\n", argv[0]);
		return 2;
	}
	if (maxmb < 1) maxmb = R201_TEXCACHE_DEFAULT_MAX_MB;

	files = (char **)calloc(PROBE_MAX_FILES, sizeof(char *));
	if (single) {
		snprintf(singleBuf, sizeof(singleBuf), "%s", dds);
		files[0] = singleBuf;
		nfiles = 1;
	} else {
		nfiles = collect_dir(ddsdir, files, PROBE_MAX_FILES);
		qsort(files, (size_t)nfiles, sizeof(char *), cmp_str);
	}

	memset(&st, 0, sizeof(st));
	probe_ensure_cache_dir();
	R201_TexCache_Begin(&g_io, 1, maxmb * 1024 * 1024);

	for (i = 0; i < nfiles; i++) {
		probe_one(files[i], &st);
	}

	printf("[R201:texcache-probe] summary files=%d hits=%d misses=%d corrupt=%d disabled=%d "
		   "store_ok=%d store_fail=%d store_skip=%d evicted=%d bytes=%lld "
		   "cpu_hash_ms=%.1f cpu_load_ms=%.1f cpu_build_ms=%.1f cpu_store_ms=%.1f cpu_total_ms=%.1f\n",
		   st.files, st.hits, st.misses, st.corrupt, st.disabled,
		   st.storeOk, st.storeFail, st.storeSkip, st.evicted, st.bytes,
		   st.hashMs, st.loadMs, st.buildMs, st.storeMs,
		   st.hashMs + st.loadMs + st.buildMs + st.storeMs);
	printf("[R201:texcache-probe] cache-base=%s target=%s maxmb=%d picmip=%d maxtex=%d mipmaps=%d\n",
		   g_cacheBase, (g_target == R201_TEXC_ASTC_4x4) ? "astc" : "etc2",
		   maxmb, g_picmip, g_maxtex, g_mipmaps);

	if (out) {
		of = fopen(out, "wb");
		if (!of || fwrite(g_out, 1, (size_t)g_outLen, of) != (size_t)g_outLen) {
			fprintf(stderr, "cannot write %s\n", out);
			return 2;
		}
		fclose(of);
		printf("[R201:texcache-probe] wrote %ld bytes to %s\n", g_outLen, out);
	}
	free(g_out);
	free(files);
	printf("[R201:texcache-probe] result: PASS\n");
	return 0;
}
