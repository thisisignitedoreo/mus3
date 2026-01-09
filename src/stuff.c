#include "stuff.h"

#include <dynamic-array.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>

bool native_little_endian(void) {
  uint32_t t = 1;
  return *((uint8_t*) &t);
}

uint32_t parse28be(uint8_t *src) {
  uint32_t num = 0;
  if (native_little_endian()) {
    for (int i = 0; i < 4; i++) num = (num << 7) | (src[i] & 0x7f);
  } else {
    for (int i = 4; i > 0; i--) num = (num << 7) | (src[i - 1] & 0x7f);
  }
  return num;
}

uint32_t parse32be(uint8_t *src) {
  uint32_t num = 0;
  if (native_little_endian()) {
    for (int i = 0; i < 4; i++) num = (num << 8) | src[i];
  } else {
    for (int i = 4; i > 0; i--) num = (num << 8) | src[i - 1];
  }
  return num;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
float parsef32be(uint8_t *src) {
  uint8_t buf[4];
  if (native_little_endian()) {
    for (int i = 0; i < 4; i++) buf[3 - i] = src[i];
  } else {
    for (int i = 0; i < 4; i++) buf[i] = src[i];
  }
  return *((float*)buf);
}
#pragma GCC diagnostic pop

uint32_t parse32le(uint8_t *src) {
  uint32_t num = 0;
  if (!native_little_endian()) {
    for (int i = 0; i < 4; i++) num = (num << 8) | src[i];
  } else {
    for (int i = 4; i > 0; i--) num = (num << 8) | src[i - 1];
  }
  return num;
}

bool read28be(FILE *f, uint32_t *dst) {
  uint8_t buf[4];
  if (!fread(buf, 4, 1, f)) return true;
  *dst = parse28be(buf);
  return false;
}

bool readf32be(FILE *f, float *dst) {
  uint8_t buf[4];
  if (!fread(buf, 4, 1, f)) return true;
  *dst = parsef32be(buf);
  return false;
}

bool read32be(FILE *f, uint32_t *dst) {
  uint8_t buf[4];
  if (!fread(buf, 4, 1, f)) return true;
  *dst = parse32be(buf);
  return false;
}

bool read32le(FILE *f, uint32_t *dst) {
  uint8_t buf[4];
  if (!fread(buf, 4, 1, f)) return true;
  *dst = parse32le(buf);
  return false;
}

uint32_t parse24be(uint8_t *src) {
  uint32_t num = 0;
  if (native_little_endian()) {
    for (int i = 0; i < 3; i++) num = (num << 8) | src[i];
  } else {
    for (int i = 3; i > 0; i--) num = (num << 8) | src[i - 1];
  }
  return num;
}

bool read24be(FILE *f, uint32_t *dst) {
  uint8_t buf[3];
  if (!fread(buf, 3, 1, f)) return true;
  *dst = parse24be(buf);
  return false;
}

uint16_t parse16be(uint8_t *src) {
  uint32_t num = 0;
  if (native_little_endian()) {
    for (int i = 0; i < 2; i++) num = (num << 8) | src[i];
  } else {
    for (int i = 2; i > 0; i--) num = (num << 8) | src[i - 1];
  }
  return num;
}

bool read16be(FILE *f, uint16_t *dst) {
  uint8_t buf[3];
  if (!fread(buf, 2, 1, f)) return true;
  *dst = parse16be(buf);
  return false;
}

bool write32be(FILE *f, uint32_t src) {
  uint8_t buf[4];
  for (int i = 4; i > 0; i--) {
    buf[i - 1] = src & 0xff;
    src = src >> 8;
  }
  return !fwrite(buf, 4, 1, f);
}

bool write16be(FILE *f, uint16_t src) {
  uint8_t buf[2];
  for (int i = 2; i > 0; i--) {
    buf[i - 1] = src & 0xff;
    src = src >> 8;
  }
  return !fwrite(buf, 2, 1, f);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
bool writef32be(FILE *f, float src) {
  uint8_t buf[4];
  if (native_little_endian()) {
    for (int i = 0; i < 4; i++) buf[3 - i] = ((uint8_t*)&src)[i];
  } else {
    for (int i = 0; i < 4; i++) buf[i] = ((uint8_t*)&src)[i];
  }
  return !fwrite(buf, 4, 1, f);
}
#pragma GCC diagnostic pop

uint32_t shuffle32(uint32_t num) {
  return
    ((num >> 24) & 0xFF) <<  0 |
    ((num >> 16) & 0xFF) <<  8 |
    ((num >>  8) & 0xFF) << 16 |
    ((num >>  0) & 0xFF) << 24;
}

uint16_t shuffle16(uint16_t num) {
  return
    ((num >> 8) & 0xFF) << 0 |
    ((num >> 0) & 0xFF) << 8;
}

void push_utf8_codepoint(char **da, uint32_t codepoint) {
  if (codepoint < 0x80) da_push(da, codepoint & 0xFF);
  else if (codepoint < 0x800) {
    da_push(da, ((codepoint >>  6) & 0x1F) | 0xC0);
    da_push(da, ((codepoint >>  0) & 0x3F) | 0x80);
  } else if (codepoint < 0x10000) {
    da_push(da, ((codepoint >> 12) & 0x0F) | 0xE0);
    da_push(da, ((codepoint >>  6) & 0x3F) | 0x80);
    da_push(da, ((codepoint >>  0) & 0x3F) | 0x80);
  } else if (codepoint < 0x800) {
    da_push(da, ((codepoint >> 18) & 0x07) | 0xF0);
    da_push(da, ((codepoint >> 12) & 0x3F) | 0x80);
    da_push(da, ((codepoint >>  6) & 0x3F) | 0x80);
    da_push(da, ((codepoint >>  0) & 0x3F) | 0x80);
  }
}

#if defined(__linux__)

#include <sys/stat.h>
#include <linux/limits.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

bool paths_equal(const char *a, const char *b) {
  struct stat st1, st2;

  if (!a || !b) return false;
  if (stat(a, &st1) != 0) return false;
  if (stat(b, &st2) != 0) return false;
  
  return st1.st_ino == st2.st_ino && st1.st_dev == st2.st_dev;
}

bool path_is_file(const char *path) {
  if (path == NULL) return false;
  struct stat path_stat;

  return stat(path, &path_stat) >= 0 && S_ISREG(path_stat.st_mode);
}

bool path_is_a_subfolder(const char *parent, const char *child) {
  char real_parent[PATH_MAX];
  char real_child[PATH_MAX];

  if (realpath(parent, real_parent) == NULL) {
    return false;
  }
  if (realpath(child, real_child) == NULL) {
    return false;
  }

  size_t parent_len = strlen(real_parent);
  if (parent_len > 0 && real_parent[parent_len - 1] != '/') {
    strcat(real_parent, "/");
    parent_len++;
  }

  if (strncmp(real_child, real_parent, parent_len) == 0) {
    return true;
  } else {
    return false;
  }
}

const char *conv_path(const char *path) {
  return path;
}

FILE *file_open(const char *path, char *mode) {
  return fopen(path, mode);
}

#elif defined(_WIN32)

// This was proudly written by ChatGPT. Fuck Win32.

#define WIN32_LEAN_AND_MEAN
#include "win32.h"
#include <shlwapi.h>

static BOOL canonicalize_a(const char *in, char *out, DWORD out_sz)
{
  char tmp[MAX_PATH];
  DWORD n = GetFullPathNameA(in, MAX_PATH, tmp, NULL);
  if (!n || n >= MAX_PATH) return FALSE;
  return PathCanonicalizeA(out, tmp);
}

static BOOL to_wide(const char *a, WCHAR *w, DWORD wcap)
{
  return MultiByteToWideChar(
			     CP_UTF8, 0,
			     a, -1,
			     w, wcap
			     ) > 0;
}

bool paths_equal(const char *a, const char *b)
{
  char ca[MAX_PATH], cb[MAX_PATH];
  WCHAR wa[MAX_PATH], wb[MAX_PATH];

  if (!canonicalize_a(a, ca, MAX_PATH)) return false;
  if (!canonicalize_a(b, cb, MAX_PATH)) return false;
  if (!to_wide(ca, wa, MAX_PATH)) return false;
  if (!to_wide(cb, wb, MAX_PATH)) return false;

  return CompareStringOrdinal(wa, -1, wb, -1, TRUE) == CSTR_EQUAL;
}

bool path_is_file(const char *path) {
  DWORD attributes = GetFileAttributesA(path);

  if (attributes == INVALID_FILE_ATTRIBUTES) {
    return false;
  }

  if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
    return true;
  }

  return false;
}

bool path_is_a_subfolder(const char *parent, const char *child)
{
  char p[MAX_PATH], c[MAX_PATH];
  WCHAR wp[MAX_PATH], wc[MAX_PATH];

  if (!canonicalize_a(parent, p, MAX_PATH)) return false;
  if (!canonicalize_a(child, c, MAX_PATH)) return false;

  size_t plen = strlen(p);
  if (p[plen - 1] != '\\') {
    p[plen++] = '\\';
    p[plen] = 0;
  }

  if (!to_wide(p, wp, MAX_PATH)) return false;
  if (!to_wide(c, wc, MAX_PATH)) return false;

  return CompareStringOrdinal(
			      wp, (int)plen,
			      wc, (int)plen,
			      TRUE
			      ) == CSTR_EQUAL;
}

const wchar_t *conv_path_(const char *utf8) {
  static wchar_t out_bufs[3][PATH_MAX];
  static int cur_buf = 0;
  cur_buf = (cur_buf + 1) % 3;
  wchar_t *out = out_bufs[cur_buf];
  if (!utf8)
    return false;

  int needed = MultiByteToWideChar(
				   CP_UTF8,
				   MB_ERR_INVALID_CHARS,
				   utf8,
				   -1,
				   NULL,
				   0
				   );

  if (needed <= 0)
    return NULL;

  if (MultiByteToWideChar(
		      CP_UTF8,
		      MB_ERR_INVALID_CHARS,
		      utf8,
		      -1,
		      out,
		      PATH_MAX
			  ) != needed) return NULL;
  return out;
}

const char *conv_path(const char *utf8) {
  return (const char*) conv_path_(utf8);
}

FILE *file_open(const char *path, char *mode) {
  return _wfopen(conv_path_(path), conv_path_(mode));
}

char *basename(char *a) {
  return PathFindFileName(a);
}

#else
#error womp womp
#endif

size_t path_filesize(const char *path) {
  FILE *f = file_open(path, "rb");
  if (f == NULL) return 0;
  fseek(f, 0, SEEK_END);
  size_t l = ftell(f);
  fclose(f);
  return l;
}

int maxi(int a, int b) {
  return a > b ? a : b;
}

char fold(char a) {
  if (ispunct(a)) return 0;
  if (isupper(a)) return tolower(a);
  else return a;
}

const char *normalize(struct allocator arena, const char *str) {
  if (str == NULL) return NULL;
  char *result = da_new(arena, char);
  for (const char *c = str; *c; c++) {
    char f = fold(*c);
    if (f) da_push(&result, f);
  }
  return result;
}

void normalize_inplace(char *dst, const char *src, int max) {
  if (src == NULL) { if (max) { *dst = 0; } return; }
  int dst_c = 0;
  for (size_t i = 0; dst_c < max && i < strlen(src); i++) {
    char f = fold(src[i]);
    if (f) dst[dst_c++] = f;
  }
}

int tokenize(const char *string, char **out, int max, int max_c) {
  int tokens = 0; int c = 0;
  for (int i = 0; tokens < max && string[i]; i++) {
    char this = string[i];
    if (isspace(this) && c > 0) {
      out[tokens][c] = 0;
      tokens++; c = 0;
    } else {
      if (c < max_c - 1) {
	char f = fold(string[i]);
	if (f) out[tokens][c++] = f;
      }
    }
  }
  if (c) {
    out[tokens][c] = 0;
    tokens++; c = 0;
  }
  return tokens;
}

int subseq_match(const char *hay, const char *needle) {
  if (hay == NULL) return -1000;
  if (needle == NULL) return -1000;

  const char *start = hay;
  
  int score = 0;
  while (*needle && *hay) {
    if (*needle == *hay) {
      score += 10;
      needle++;
    } else {
      score -= 1;
    }
    
    if (hay == start || *(hay-1) == ' ') score += 20;
    hay++;
  }
  
  if (*hay == ' ' || *hay == 0) score += 20;
  return *needle ? -1000 : score;
}

int song_match(struct music_song song, const char **input, int tokens) {
  int total = 0;

  for (int i = 0; i < tokens; i++) {
    int best = -1000;

    best = maxi(best, subseq_match(song.normalized.title, input[i]));
    best = maxi(best, subseq_match(song.normalized.artist, input[i]));
    best = maxi(best, subseq_match(song.normalized.album.title, input[i]));
    best = maxi(best, subseq_match(song.normalized.album.artist, input[i]));
    best = maxi(best, song.year == atoi(input[i]) ? 50 : -1000);

    if (best < 0) return -1;

    total += best;
  }
  return total;
}

int album_match(struct music_album album, const char **input, int tokens) {
  int total = 0;

  for (int i = 0; i < tokens; i++) {
    int best = -1000;

    best = maxi(best, subseq_match(album.normalized.title, input[i]) * 2);
    best = maxi(best, subseq_match(album.normalized.artist, input[i]));
    best = maxi(best, album.year == atoi(input[i]) ? 50 : -1000);

    if (best < 0) return -1;

    total += best;
  }
  return total;
}
