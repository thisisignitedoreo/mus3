#pragma once

#include <allocator.h>
#include <miniaudio.h>
#include <stdbool.h>

struct music_cover {
  bool exists;
  const char *ext;
  size_t offset, size;
  const char *path;
  int texture;
};

struct music_song {
  const char *path;

  struct music_cover cover;
  
  const char *title, *artist;
  struct {
    const char *title, *artist;
  } album;
  int year;
  struct {
    int this, all;
  } track, disc;

  struct {
    const char *title, *artist;
    struct {
      const char *title, *artist;
    } album;
  } normalized;

  uint64_t length; // ms
 
  bool ephemeral;
};

struct music_album {
  const char *title, *artist;
  struct {
    const char *title, *artist;
  } normalized;

  int tracks, discs, year;

  struct music_cover cover;
  
  uint64_t length;

  struct music_song *tracklist;
};

struct music_context {
  struct allocator arena;
  ma_engine engine;

  bool had_error;
  ma_sound current_song;
  int current_song_idx;

  enum music_looping {
    MUSIC_LINEAR,
    MUSIC_REPEAT_PLAYLIST,
    MUSIC_REPEAT_ONE,
    MUSIC_SHUFFLE,
    MUSIC_LOOPING_COUNT
  } looping;

  const char **load_queue;
  
  struct music_song *song_cache;
  
  struct music_scan_folder {
    const char *path;
    bool scanned;
  } *scan_folders;
  
  struct music_album *albums;
  struct music_song *playlist;

  const char **play_history;

  bool should_quit;
};

bool setup_music(struct music_context *ctx, struct allocator arena);
void destroy_music(struct music_context *ctx);

void music_tick(struct music_context *ctx, bool media_inited);

int music_preload(struct music_context *ctx, const char *path, bool ephemeral);

bool music_is_load_queued(struct music_context *ctx);
void music_load_one_from_queue(struct music_context *ctx);

void music_set_looping(struct music_context *ctx, enum music_looping looping);

void music_playlist_push(struct music_context *ctx, const char *path);
void music_playlist_pop(struct music_context *ctx, int idx);

void music_playlist_next(struct music_context *ctx);
void music_playlist_prev(struct music_context *ctx);
bool music_playlist_can_next(struct music_context *ctx);
bool music_playlist_can_prev(struct music_context *ctx);

void music_scan_folder(struct music_context *ctx, const char *path);

float music_get_volume(struct music_context *ctx);
void music_set_volume(struct music_context *ctx, float vol);

void music_load(struct music_context *ctx, const char *path);
void music_unload(struct music_context *ctx);
bool music_get_pause(struct music_context *ctx);
void music_set_pause(struct music_context *ctx, bool pause_state);
float music_get_length(struct music_context *ctx);
float music_get_seek(struct music_context *ctx);
void music_set_seek(struct music_context *ctx, float seek);

