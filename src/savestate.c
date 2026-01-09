#include "savestate.h"

#include "stuff.h"

#include <dynamic-array.h>

#include <string.h>

#ifdef __linux__
#include <linux/limits.h>
#include <stdlib.h>
#endif

#define SAVESTATE_VERSION 0

const char *get_savestate_folder(void) {
#if __linux__
  static char path[PATH_MAX];
  const char *home = getenv("HOME");
  snprintf(path, PATH_MAX, "%s/.config/mus3/", home);
  return path;
#else
  return "./";
#endif
}

const char *get_savestate_file(void) {
#if __linux__
  static char path[PATH_MAX];
  const char *home = getenv("HOME");
  snprintf(path, PATH_MAX, "%s/.config/mus3/mus3.savestate", home);
  return path;
#else
  return "mus3.savestate";
#endif
}

static inline long start_section(FILE *f, const char *name) {
  if (!fwrite(name, 4, 1, f)) return 0;
  long loc = ftell(f);
  uint32_t zero = 0;
  if (!fwrite(&zero, 4, 1, f)) return 0;
  return loc;
}

static inline bool end_section(FILE *f, long loc) {
  long current_loc = ftell(f);
  uint32_t length = ftell(f) - loc - 4;
  if (fseek(f, loc, SEEK_SET)) return true;
  if (write32be(f, length)) return true;
  fseek(f, current_loc, SEEK_SET);
  return false;
}

bool save_to_savestate(struct music_context *ctx, struct ui_context *ui, const char *path) {
  bool error = true;
  FILE *f = file_open(path, "wb");
  if (f == NULL) goto return_error;
  if (!fwrite("MUS3", 4, 1, f)) goto close_file;
  if (write16be(f, SAVESTATE_VERSION)) goto close_file;

  long this_sect = 0;

  if (music_get_volume(ctx) != 1.0f) {
    if ((this_sect = start_section(f, "VOLU")) == 0) goto close_file;
    if (writef32be(f, music_get_volume(ctx))) goto close_file;
    if (end_section(f, this_sect)) goto close_file;
  }

  if (da_size(&ctx->playlist)) {
    if ((this_sect = start_section(f, "PLAY")) == 0) goto close_file;

    if (write16be(f, da_size(&ctx->playlist))) goto close_file;
    for (size_t i = 0; i < da_size(&ctx->playlist); i++) {
      if (write32be(f, strlen(ctx->playlist[i].path))) goto close_file;
      if (!fwrite(ctx->playlist[i].path, strlen(ctx->playlist[i].path), 1, f)) goto close_file;
    }

    if (ctx->current_song_idx < 0) {
      if (write16be(f, (uint16_t)(-1))) goto close_file;
    } else {
      for (size_t i = 0; i < da_size(&ctx->playlist); i++) {
	if (paths_equal(ctx->playlist[i].path, ctx->song_cache[ctx->current_song_idx].path)) {
	  if (write16be(f, (uint16_t)(i))) goto close_file;
	  break;
	}
      }
      
      if (writef32be(f, music_get_seek(ctx))) goto close_file;
      bool paused = music_get_pause(ctx);
      if (!fwrite(&paused, 1, 1, f)) goto close_file;
      if (write16be(f, (uint16_t) ctx->looping)) goto close_file;
    }
    
    if (end_section(f, this_sect)) goto close_file;
  }
  
  if (da_size(&ctx->scan_folders)) {
    if ((this_sect = start_section(f, "SCAN")) == 0) goto close_file;

    for (size_t i = 0; i < da_size(&ctx->scan_folders); i++) {
      if (write32be(f, strlen(ctx->scan_folders[i].path))) goto close_file;
      if (!fwrite(ctx->scan_folders[i].path, strlen(ctx->scan_folders[i].path), 1, f)) goto close_file;
    }

    if (end_section(f, this_sect)) goto close_file;
  }
  
  if ((this_sect = start_section(f, "SETT")) == 0) goto close_file;

  uint8_t settings =
    (ui->settings.dont_load_covers ? 1 : 0) << 0;
  
  if (!fwrite(&settings, 1, 1, f)) goto close_file;

  if (end_section(f, this_sect)) goto close_file;
  
  if (!fwrite("CAKE\0\0\0\0", 8, 1, f)) goto close_file;

  error = false;
 close_file:
  fclose(f);
 return_error:
  return error;
}

bool load_from_savestate(struct music_context *ctx, struct ui_context *ui, const char *path) {
  bool error = true;
  FILE *f = file_open(path, "rb");
  if (f == NULL) goto return_error;

  uint8_t magic[4];
  if (!fread(magic, 4, 1, f) || memcmp(magic, "MUS3", 4) != 0) goto close_file;

  uint16_t version; if (read16be(f, &version)) goto close_file;
  if (version > SAVESTATE_VERSION) goto close_file;

  char sect_name[4]; uint32_t sect_length;
  while (true) {
    if (!fread(sect_name, 4, 1, f)) goto close_file;
    if (read32be(f, &sect_length)) goto close_file;
    if (memcmp(sect_name, "CAKE", 4) == 0 && sect_length == 0) break;

    long end = ftell(f) + sect_length;

    if (memcmp(sect_name, "SCAN", 4) == 0) {
      uint32_t length;
      while (ftell(f) < end) {
	if (read32be(f, &length)) goto close_file;
	char *path = a_malloc(ctx->arena, length + 1);
	if (path == NULL) goto close_file;
	path[length] = 0;
	if (!fread(path, length, 1, f)) goto close_file;
	music_scan_folder(ctx, path);
      }
    }
    if (memcmp(sect_name, "VOLU", 4) == 0) {
      float volume;
      if (readf32be(f, &volume)) goto close_file;
      music_set_volume(ctx, volume);
    }
    if (memcmp(sect_name, "PLAY", 4) == 0) {
      uint16_t playlist_size;
      if (read16be(f, &playlist_size)) goto close_file;
      uint32_t length;
      for (uint16_t i = 0; i < playlist_size; i++) {
	if (read32be(f, &length)) goto close_file;
	char *path = a_malloc(ctx->arena, length + 1);
	if (path == NULL) goto close_file;
	path[length] = 0;
	if (!fread(path, length, 1, f)) goto close_file;
	music_playlist_push(ctx, path);
      }
      uint16_t playing;
      if (read16be(f, &playing)) goto close_file;
      if (playing != (uint16_t)(-1)) {
	music_load(ctx, ctx->playlist[playing].path);
	float seek = 0.0f;
	bool pause = false;
        uint16_t looping = 0;
	if (readf32be(f, &seek)) goto close_file;
	if (!fread(&pause, 1, 1, f)) goto close_file;
	if (read16be(f, &looping)) goto close_file;
	music_set_seek(ctx, seek);
	music_set_pause(ctx, pause);
	music_set_looping(ctx, (enum music_looping) looping);
      }
    }
    if (memcmp(sect_name, "SETT", 4) == 0) {
      uint8_t settings;
      if (!fread(&settings, 1, 1, f)) goto close_file;
      ui->settings.dont_load_covers = settings & (1 << 0);
    }

    if (fseek(f, end, SEEK_SET)) goto close_file;
  }
  
  error = false;
 close_file:
  fclose(f);
 return_error:
  if (error) puts("Savefile parsing error!");
  return error;
}
