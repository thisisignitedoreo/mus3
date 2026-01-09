#include <stdio.h>
#include <stdint.h>

#include "music.h"

bool native_little_endian(void);

uint32_t parse32be(uint8_t *src);
float parsef32be(uint8_t *src);
uint32_t parse32le(uint8_t *src);
uint32_t parse28be(uint8_t *src); // IDv2 strange unsync int stuff
uint32_t parse24be(uint8_t *src);
uint16_t parse16be(uint8_t *src);
bool read32be(FILE *f, uint32_t *dst);
bool readf32be(FILE *f, float *dst);
bool read32le(FILE* f, uint32_t *dst);
bool read28be(FILE* f, uint32_t *dst);
bool read24be(FILE* f, uint32_t *dst);
bool read16be(FILE *f, uint16_t *dst);

uint32_t shuffle32(uint32_t num); // 32b big endian <-> little endian
uint16_t shuffle16(uint16_t num); // 16b big endian <-> little endian

bool write32be(FILE *f, uint32_t src);
bool write16be(FILE *f, uint16_t src);
bool writef32be(FILE *f, float src);

void push_utf8_codepoint(char **da, uint32_t codepoint);

bool paths_equal(const char *a, const char *b);
bool path_is_a_subfolder(const char *parent, const char *child);
bool path_is_file(const char *path);
size_t path_filesize(const char *path);
#ifdef _WIN32
const wchar_t *conv_path_(const char *path);
char *basename(char *a);
#endif
const char *conv_path(const char *path);
FILE *file_open(const char *path, char *mode);

int maxi(int a, int b);

char fold(char a);
const char *normalize(struct allocator arena, const char *str);
void normalize_inplace(char *dst, const char *src, int max);
int tokenize(const char *string, char **out, int max, int max_c);
int subseq_match(const char *hay, const char *needle);
int song_match(struct music_song song, const char **input, int tokens);
int album_match(struct music_album album, const char **input, int tokens);
