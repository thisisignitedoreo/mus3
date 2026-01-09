#include <miniaudio.h>

#include <dynamic-array.h>

#include "music.h"
#include "media.h"

#include "stuff.h"
#include "tags.h"

#include <string.h>
#include <stdlib.h>

#pragma GCC diagnostic ignored "-Wmissing-braces"

bool setup_music(struct music_context *ctx, struct allocator arena) {
  *ctx = (struct music_context) {
    .arena = arena,
    .engine = {0},

    .had_error = false,
    .current_song = NULL,
    .current_song_idx = -1,
  };
  
  ma_engine_config cfg = ma_engine_config_init();
  if (ma_engine_init(&cfg, &ctx->engine) != MA_SUCCESS) {
    return true;
  }

  ctx->load_queue = da_new(arena, const char*);
  ctx->song_cache = da_new(arena, struct music_song);
  ctx->scan_folders = da_new(arena, struct music_scan_folder);
  ctx->albums = da_new(arena, struct music_album);
  ctx->playlist = da_new(arena, struct music_song);
  ctx->play_history = da_new(arena, const char*);
  return false;
}

void destroy_music(struct music_context *ctx) {
  if (ctx->current_song_idx >= 0) ma_sound_uninit(&ctx->current_song);
  ma_engine_uninit(&ctx->engine);
}

void music_tick(struct music_context *ctx, bool media_inited) {
  if (ctx->current_song_idx >= 0 && ma_sound_at_end(&ctx->current_song)) {
    music_playlist_next(ctx);
  }

  if (media_inited) {
    enum media_event event;
    while ((event = media_tick()) != EVENT_NONE) {
      if (event == EVENT_ERROR) break;
      // pass!
    }
  }
}

#define SORT_ALBUM(i) do {						\
    for (size_t t = 0; t < da_size(&ctx->albums[i].tracklist); t++) {	\
      for (size_t track = 0; track < da_size(&ctx->albums[i].tracklist) - 1; track++) { \
	struct music_song a = ctx->albums[i].tracklist[track];		\
	struct music_song b = ctx->albums[i].tracklist[track + 1];	\
	if (a.track.this > b.track.this) {				\
	  ctx->albums[i].tracklist[track] = b;				\
	  ctx->albums[i].tracklist[track + 1] = a;			\
	}								\
      }									\
    }									\
    for (size_t t = 0; t < da_size(&ctx->albums[i].tracklist); t++) {	\
      for (size_t track = 0; track < da_size(&ctx->albums[i].tracklist) - 1; track++) { \
	struct music_song a = ctx->albums[i].tracklist[track];		\
	struct music_song b = ctx->albums[i].tracklist[track + 1];	\
	if (a.disc.this > b.disc.this) {				\
	  ctx->albums[i].tracklist[track] = b;				\
	  ctx->albums[i].tracklist[track + 1] = a;			\
	}								\
      }									\
    }									\
  } while (0)

int music_preload(struct music_context *ctx, const char *path, bool ephemeral) {
  for (size_t i = 0; i < da_size(&ctx->song_cache); i++) {
    if (paths_equal(path, ctx->song_cache[i].path)) {
      if (ctx->song_cache[i].ephemeral && !ephemeral) {
	struct music_song song = ctx->song_cache[i];

	if (song.title != NULL) {
	  for (size_t j = 0; j < da_size(&ctx->albums); j++) {
	    struct music_album album = ctx->albums[j];
	    if (strcmp(album.title, song.album.title) == 0 && strcmp(album.artist, song.album.artist) == 0) {
	      da_push(&ctx->albums[j].tracklist, song);
	      ctx->albums[j].length += song.length;
	      SORT_ALBUM(j);
	    
	      ctx->song_cache[i].ephemeral = false;
	      return (int) i;
	    }
	  }

	  struct music_album album = {
	    song.album.title, song.album.artist,
	    { song.normalized.album.title, song.normalized.album.artist },
	    song.track.all, song.disc.all, song.year,
	    song.cover,
	    song.length,
	    da_new(ctx->arena, struct music_song),
	  };
	  da_push(&album.tracklist, song);

	  da_push(&ctx->albums, album);
	}
	
	ctx->song_cache[i].ephemeral = false;
      }
      return (int) i;
    }
  }

  struct music_song song = tags_load(ctx->arena, path);
  song.ephemeral = ephemeral;
  da_push(&ctx->song_cache, song);

  if (!(song.title && song.artist && song.album.title && song.album.artist)) return da_size(&ctx->song_cache) - 1;

  if (!ephemeral && song.title != NULL) {
    for (size_t i = 0; i < da_size(&ctx->albums); i++) {
      struct music_album album = ctx->albums[i];
      if (strcmp(album.title, song.album.title) == 0 && strcmp(album.artist, song.album.artist) == 0) {
	da_push(&ctx->albums[i].tracklist, song);
	ctx->albums[i].length += song.length;
	SORT_ALBUM(i);
	return da_size(&ctx->song_cache) - 1;
      }
    }

    struct music_album album = {
      song.album.title, song.album.artist,
      { song.normalized.album.title, song.normalized.album.artist },
      song.track.all, song.disc.all, song.year,
      song.cover,
      song.length,
      da_new(ctx->arena, struct music_song),
    };
    da_push(&album.tracklist, song);

    da_push(&ctx->albums, album);
  }
  
  return da_size(&ctx->song_cache) - 1;
}

bool music_is_load_queued(struct music_context *ctx) {
  return da_size(&ctx->load_queue) > 0;
}

void music_load_one_from_queue(struct music_context *ctx) {
  if (da_size(&ctx->load_queue) == 0) return;
  music_preload(ctx, ctx->load_queue[da_size(&ctx->load_queue) - 1], false);
  da_size(&ctx->load_queue) -= 1;
}

void music_set_looping(struct music_context *ctx, enum music_looping looping) {
  ctx->looping = looping;
  media_update(CHANGE_LOOP_MODE);
}

void music_playlist_push(struct music_context *ctx, const char *path) {
  int id = music_preload(ctx, path, true);
  if (id < 0) return;
  for (size_t i = 0; i < da_size(&ctx->playlist); i++) if (paths_equal(ctx->playlist[i].path, path)) return;
  da_push(&ctx->playlist, ctx->song_cache[id]);
}

// TODO: implement da_remove
void music_playlist_pop(struct music_context *ctx, int idx) {
  for (size_t i = da_size(&ctx->play_history); i > 0; i--) {
    if (paths_equal(ctx->play_history[i-1], ctx->playlist[idx].path)) {
      memmove(ctx->play_history + i - 1, ctx->play_history + i, (da_size(&ctx->play_history) - i) * sizeof(ctx->play_history[0]));
      da_size(&ctx->play_history) -= 1;
    }
  }
  if (paths_equal(ctx->playlist[idx].path, ctx->song_cache[ctx->current_song_idx].path)) {
    music_unload(ctx);
  }
  memmove(ctx->playlist + idx, ctx->playlist + idx + 1, (da_size(&ctx->playlist) - idx - 1) * sizeof(struct music_song));
  da_size(&ctx->playlist) -= 1;
}

void music_playlist_next(struct music_context *ctx) {
  if (ctx->current_song_idx < 0) return;
  int playlist_indice = -1;
  const char *path = ctx->song_cache[ctx->current_song_idx].path;
  for (size_t i = 0; i < da_size(&ctx->playlist); i++) {
    if (paths_equal(ctx->playlist[i].path, path)) playlist_indice = i;
  }
  if (playlist_indice < 0) return;
  music_unload(ctx);
  int next_playlist_indice = -1;
  if (ctx->looping == MUSIC_LINEAR && (size_t) playlist_indice < da_size(&ctx->playlist) - 1) next_playlist_indice = playlist_indice + 1;
  else if (ctx->looping == MUSIC_REPEAT_PLAYLIST) next_playlist_indice = (playlist_indice + 1) % da_size(&ctx->playlist);
  else if (ctx->looping == MUSIC_REPEAT_ONE) next_playlist_indice = playlist_indice;
  else if (ctx->looping == MUSIC_SHUFFLE) next_playlist_indice = rand() % da_size(&ctx->playlist);
  if (next_playlist_indice >= 0) {
    music_load(ctx, ctx->playlist[next_playlist_indice].path);
    da_push(&ctx->play_history, ctx->playlist[playlist_indice].path);
  }
}

void music_playlist_prev(struct music_context *ctx) {
  if (da_size(&ctx->play_history) == 0) return;
  if (ctx->current_song_idx >= 0) music_unload(ctx);
  music_load(ctx, da_pop(&ctx->play_history));
}

bool music_playlist_can_next(struct music_context *ctx) {
  int playlist_indice = -1;
  const char *path = ctx->song_cache[ctx->current_song_idx].path;
  for (size_t i = 0; i < da_size(&ctx->playlist); i++) if (paths_equal(ctx->playlist[i].path, path)) playlist_indice = i;
  if (playlist_indice < 0) return false;
  
  if (ctx->looping == MUSIC_LINEAR) return (size_t) playlist_indice < da_size(&ctx->playlist) - 1;
  return true;
}

bool music_playlist_can_prev(struct music_context *ctx) {
  return da_size(&ctx->play_history) > 0;
}

#include <raylib.h>
void music_scan_folder(struct music_context *ctx, const char *path) {
  for (size_t i = 0; i < da_size(&ctx->scan_folders); i++) {
    if (path_is_a_subfolder(ctx->scan_folders[i].path, path) && ctx->scan_folders[i].scanned) {
      struct music_scan_folder folder = { path, true };
      da_push(&ctx->scan_folders, folder);
      return;
    }
    if (paths_equal(ctx->scan_folders[i].path, path)) {
      if (ctx->scan_folders[i].scanned) return;
      else {
	ctx->scan_folders[i].scanned = true;
	goto dont_push;
      }
    }
  }
  
  struct music_scan_folder folder = { path, true };
  da_push(&ctx->scan_folders, folder);
 dont_push:
  (void)0; // C stuff *shrug*

  // TODO: replace this with my code (don't depend on raylib in the music module)
  FilePathList paths = LoadDirectoryFilesEx(path, "mp3;flac", true);
  for (unsigned int i = 0; i < paths.count; i++) {
    size_t pathlen = strlen(paths.paths[i]);
    char *path = a_malloc(ctx->arena, pathlen + 1);
    strcpy(path, paths.paths[i]);
    music_preload(ctx, path, false);
  }
}

float music_get_volume(struct music_context *ctx) {
  return ma_engine_get_volume(&ctx->engine);
}

void music_set_volume(struct music_context *ctx, float vol) {
  media_update(CHANGE_VOLUME);
  ma_engine_set_volume(&ctx->engine, vol);
}

void music_load(struct music_context *ctx, const char *path) {
  if (ctx->current_song_idx >= 0) music_unload(ctx);
  int idx = music_preload(ctx, path, true);
#ifdef _WIN32
  ma_sound_init_from_file_w(&ctx->engine, conv_path_(path), MA_SOUND_FLAG_STREAM, NULL, NULL, &ctx->current_song);
#else
  ma_sound_init_from_file(&ctx->engine, path, MA_SOUND_FLAG_STREAM, NULL, NULL, &ctx->current_song);
#endif
  ma_sound_start(&ctx->current_song);
  ctx->current_song_idx = idx;
  media_update(CHANGE_ALL);
}

void music_unload(struct music_context *ctx) {
  if (ctx->current_song_idx < 0) return;
  ma_sound_uninit(&ctx->current_song);
  ctx->current_song_idx = -1;
  media_update(CHANGE_ALL);
}

bool music_get_pause(struct music_context *ctx) {
  if (ctx->current_song_idx < 0) return false;
  return !ma_sound_is_playing(&ctx->current_song);
}

void music_set_pause(struct music_context *ctx, bool pause_state) {
  if (ctx->current_song_idx < 0) return;
  if (pause_state) ma_sound_stop(&ctx->current_song);
  else ma_sound_start(&ctx->current_song);
  media_update(CHANGE_PLAYING);
}

float music_get_length(struct music_context *ctx) {
  if (ctx->current_song_idx < 0) return 0.0f;
  float length; ma_sound_get_length_in_seconds(&ctx->current_song, &length);
  return length;
}

float music_get_seek(struct music_context *ctx) {
  if (ctx->current_song_idx < 0) return 0.0f;
  float cursor; ma_sound_get_cursor_in_seconds(&ctx->current_song, &cursor);
  return cursor;
}

void music_set_seek(struct music_context *ctx, float seek) {
  if (ctx->current_song_idx < 0) return;
  ma_sound_seek_to_second(&ctx->current_song, seek);
  media_update(CHANGE_SEEK);
}
