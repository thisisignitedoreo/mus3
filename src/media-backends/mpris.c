// Don't look here
// This sucks balls so hard I am surprised it's even working

#include "media.h"

#include <dynamic-array.h>

#include <sd-bus.h>

static sd_bus *bus = NULL;
static sd_bus_slot *slot1 = NULL, *slot2 = NULL;

// org.mpris.MediaPlayer2

static int method_Next(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  if (music_playlist_can_next(ctx)) music_playlist_next(ctx);
  return sd_bus_reply_method_return(m, "");
}

static int method_Previous(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  if (music_playlist_can_prev(ctx)) music_playlist_prev(ctx);
  return sd_bus_reply_method_return(m, "");
}

static int method_Pause(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  music_set_pause(ctx, true);
  return sd_bus_reply_method_return(m, "");
}

static int method_PlayPause(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  if (ctx->current_song_idx < 0) return sd_bus_error_setf(ret_error,
							  SD_BUS_ERROR_NOT_SUPPORTED,
							  "Can't play/pause when stopped");
  music_set_pause(ctx, !music_get_pause(ctx));
  return sd_bus_reply_method_return(m, "");
}

static int method_Stop(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  music_unload(ctx);
  return sd_bus_reply_method_return(m, "");
}

static int method_Play(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  music_set_pause(ctx, false);
  return sd_bus_reply_method_return(m, "");
}

static int method_Seek(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  int64_t seek;
  int r = sd_bus_message_read(m, "x", &seek); if (r < 0) return r;
  music_set_seek(ctx, seek / 1e6f);
  return sd_bus_reply_method_return(m, "");
}

static int method_SetPosition(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  int64_t seek; const char *id;
  int r = sd_bus_message_read(m, "ox", &id, &seek); if (r < 0) return r;
  if (strcmp(id, a_sprintf(ctx->arena, "/org/mpris/MediaPlayer2/track/%d", ctx->current_song_idx)) != 0) return sd_bus_reply_method_return(m, "");
  music_set_seek(ctx, seek / 1e6f);
  return sd_bus_reply_method_return(m, "");
}

static int method_OpenUri(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  const char *uri;
  int r = sd_bus_message_read(m, "s", &uri); if (r < 0) return r;
  if (strlen(uri) < 7 || memcmp(uri, "file://", 7) != 0) return sd_bus_error_setf(ret_error,
										  SD_BUS_ERROR_FAILED,
										  "Expected file:// URI");
  uri += 7;
  char *owned_path = a_malloc(ctx->arena, strlen(uri) + 1);
  if (owned_path == NULL) return sd_bus_error_setf(ret_error,
						   SD_BUS_ERROR_NO_MEMORY,
						   "OOM");
  memcpy(owned_path, uri, strlen(uri) + 1);
  music_playlist_push(ctx, owned_path);
  return sd_bus_reply_method_return(m, "");
}

static int property_GetPlaybackStatus(sd_bus *bus, const char *path,
                                      const char *interface, const char *property,
                                      sd_bus_message *reply, void *userdata,
                                      sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  return sd_bus_message_append(reply, "s", ctx->current_song_idx < 0 ? "Stopped" : music_get_pause(ctx) ? "Paused" : "Playing");
}

static int property_GetLoopStatus(sd_bus *bus, const char *path,
				  const char *interface, const char *property,
				  sd_bus_message *reply, void *userdata,
				  sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  const char *status = "None";
  if (ctx->looping == MUSIC_LINEAR) status = "None";
  else if (ctx->looping == MUSIC_REPEAT_PLAYLIST) status = "Playlist";
  else if (ctx->looping == MUSIC_REPEAT_ONE) status = "Track";
  return sd_bus_message_append(reply, "s", status);
}

static int property_SetLoopStatus(sd_bus *bus, const char *path,
				  const char *interface, const char *property,
				  sd_bus_message *reply, void *userdata,
				  sd_bus_error *ret_error) {
  const char *status; int r = sd_bus_message_read(reply, "s", &status);
  if (r < 0) return r;
  
  struct music_context *ctx = userdata;

  if (strcmp(status, "None") == 0) music_set_looping(ctx, MUSIC_LINEAR);
  else if (strcmp(status, "Playlist") == 0) music_set_looping(ctx, MUSIC_REPEAT_PLAYLIST);
  else if (strcmp(status, "Track") == 0) music_set_looping(ctx, MUSIC_REPEAT_ONE);
  
  return 0;
}

static int property_GetRate(sd_bus *bus, const char *path,
			    const char *interface, const char *property,
			    sd_bus_message *reply, void *userdata,
			    sd_bus_error *ret_error) {
  return sd_bus_message_append(reply, "d", 1.0);
}

static int property_SetRate(sd_bus *bus, const char *path,
			    const char *interface, const char *property,
			    sd_bus_message *reply, void *userdata,
			    sd_bus_error *ret_error) {
  return sd_bus_error_setf(ret_error,
			   SD_BUS_ERROR_ACCESS_DENIED,
			   "Property '%s' is read-only", property);
}

static int property_GetShuffle(sd_bus *bus, const char *path,
			       const char *interface, const char *property,
			       sd_bus_message *reply, void *userdata,
			       sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  return sd_bus_message_append(reply, "b", ctx->looping == MUSIC_SHUFFLE);
}

static int property_SetShuffle(sd_bus *bus, const char *path,
			       const char *interface, const char *property,
			       sd_bus_message *reply, void *userdata,
			       sd_bus_error *ret_error) {
  int status; int r = sd_bus_message_read(reply, "b", &status);
  if (r < 0) return r;
  struct music_context *ctx = userdata;
  music_set_looping(ctx, status ? MUSIC_SHUFFLE : MUSIC_LINEAR);
  return 0;
}

// Oh god
static int property_GetMetadata(sd_bus *bus, const char *path,
                                const char *interface, const char *property,
                                sd_bus_message *reply, void *userdata,
                                sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  int r = sd_bus_message_open_container(reply, 'a', "{sv}"); if (r < 0) return r;
  r = sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv"); if (r < 0) return r;
  r = sd_bus_message_append(reply, "s", "mpris:trackid"); if (r < 0) return r;
  r = sd_bus_message_open_container(reply, 'v', "o"); if (r < 0) return r;
  if (ctx->current_song_idx >= 0) {
    r = sd_bus_message_append(reply, "o", a_sprintf(ctx->arena, "/org/mpris/MediaPlayer2/track/%d", ctx->current_song_idx)); if (r < 0) return r;
  } else {
    r = sd_bus_message_append(reply, "o", "/org/mpris/MediaPlayer2/TrackList/NoTrack"); if (r < 0) return r;
  }
  r = sd_bus_message_close_container(reply); if (r < 0) return r;
  r = sd_bus_message_close_container(reply); if (r < 0) return r;

  if (ctx->current_song_idx >= 0) {
    struct music_song song = ctx->song_cache[ctx->current_song_idx];
    if (song.cover.exists && song.cover.offset == 0) {
      r = sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv"); if (r < 0) return r;
      r = sd_bus_message_append(reply, "s", "mpris:artUrl"); if (r < 0) return r;
      r = sd_bus_message_open_container(reply, 'v', "s"); if (r < 0) return r;
      r = sd_bus_message_append(reply, "s", a_sprintf(ctx->arena, "file://%s", song.cover.path)); if (r < 0) return r;
      r = sd_bus_message_close_container(reply); if (r < 0) return r;
      r = sd_bus_message_close_container(reply); if (r < 0) return r;
    }
    r = sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", "mpris:length"); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, 'v', "x"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "x", (int64_t) (song.length * 1000)); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", "xesam:album"); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, 'v', "s"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", song.album.title); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", "xesam:albumArtist"); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, 'v', "as"); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, 'a', "s"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", song.album.artist); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", "xesam:artist"); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, 'v', "as"); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, 'a', "s"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", song.artist); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", "xesam:discNumber"); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, 'v', "i"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "i", song.disc.this); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", "xesam:title"); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, 'v', "s"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", song.title); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", "xesam:trackNumber"); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, 'v', "i"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "i", song.track.this); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", "xesam:url"); if (r < 0) return r;
    r = sd_bus_message_open_container(reply, 'v', "s"); if (r < 0) return r;
    r = sd_bus_message_append(reply, "s", a_sprintf(ctx->arena, "file://%s", song.path)); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
    r = sd_bus_message_close_container(reply); if (r < 0) return r;
  }

  r = sd_bus_message_close_container(reply); if (r < 0) return r;
  return 0;
}

static int property_GetVolume(sd_bus *bus, const char *path,
			      const char *interface, const char *property,
			      sd_bus_message *reply, void *userdata,
			      sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  return sd_bus_message_append(reply, "d", music_get_volume(ctx));
}

static int property_SetVolume(sd_bus *bus, const char *path,
			      const char *interface, const char *property,
			      sd_bus_message *reply, void *userdata,
			      sd_bus_error *ret_error) {
  float volume; int r = sd_bus_message_read(reply, "d", &volume);
  if (r < 0) return r;
  struct music_context *ctx = userdata;
  music_set_volume(ctx, volume < 0.0f ? 0.0f : volume > 1.0f ? 1.0f : volume);
  return 0;
}

static int property_GetPosition(sd_bus *bus, const char *path,
                                const char *interface, const char *property,
                                sd_bus_message *reply, void *userdata,
                                sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  return sd_bus_message_append(reply, "x", ctx->current_song_idx < 0 ? 0 : (int64_t) (music_get_seek(ctx) * 1e6f));
}

static int property_GetMaximumRate(sd_bus *bus, const char *path,
				   const char *interface, const char *property,
				   sd_bus_message *reply, void *userdata,
				   sd_bus_error *ret_error) {
  return sd_bus_message_append(reply, "d", 1.0f);
}

static int property_GetMinimumRate(sd_bus *bus, const char *path,
				   const char *interface, const char *property,
				   sd_bus_message *reply, void *userdata,
				   sd_bus_error *ret_error) {
  return sd_bus_message_append(reply, "d", 1.0f);
}

static int property_GetCanGoNext(sd_bus *bus, const char *path,
				 const char *interface, const char *property,
				 sd_bus_message *reply, void *userdata,
				 sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  return sd_bus_message_append(reply, "b", music_playlist_can_next(ctx));
}

static int property_GetCanGoPrevious(sd_bus *bus, const char *path,
				     const char *interface, const char *property,
				     sd_bus_message *reply, void *userdata,
				     sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  return sd_bus_message_append(reply, "b", music_playlist_can_prev(ctx));
}

static int property_GetCanPlay(sd_bus *bus, const char *path,
			       const char *interface, const char *property,
			       sd_bus_message *reply, void *userdata,
			       sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  return sd_bus_message_append(reply, "b", ctx->current_song_idx >= 0);
}

static int property_GetCanPause(sd_bus *bus, const char *path,
				const char *interface, const char *property,
				sd_bus_message *reply, void *userdata,
				sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  return sd_bus_message_append(reply, "b", ctx->current_song_idx >= 0);
}

static int property_GetCanSeek(sd_bus *bus, const char *path,
			       const char *interface, const char *property,
			       sd_bus_message *reply, void *userdata,
			       sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  return sd_bus_message_append(reply, "b", ctx->current_song_idx >= 0);
}

static int property_GetCanControl(sd_bus *bus, const char *path,
				  const char *interface, const char *property,
				  sd_bus_message *reply, void *userdata,
				  sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  return sd_bus_message_append(reply, "b", ctx->current_song_idx >= 0);
}

// org.mpris.MediaPlayer2.Player

static int property_GetCanQuit(sd_bus *bus, const char *path,
			       const char *interface, const char *property,
			       sd_bus_message *reply, void *userdata,
			       sd_bus_error *ret_error) {
  return sd_bus_message_append(reply, "b", true);
}

static int property_GetFullscreen(sd_bus *bus, const char *path,
				  const char *interface, const char *property,
				  sd_bus_message *reply, void *userdata,
				  sd_bus_error *ret_error) {
  return sd_bus_message_append(reply, "b", false);
}

static int property_SetFullscreen(sd_bus *bus, const char *path,
				  const char *interface, const char *property,
				  sd_bus_message *reply, void *userdata,
				  sd_bus_error *ret_error) {
  return sd_bus_error_setf(ret_error,
			   SD_BUS_ERROR_ACCESS_DENIED,
			   "Property '%s' is read-only", property);
}

static int property_GetCanSetFullscreen(sd_bus *bus, const char *path,
					const char *interface, const char *property,
					sd_bus_message *reply, void *userdata,
					sd_bus_error *ret_error) {
  return sd_bus_message_append(reply, "b", false);
}

static int property_GetCanRaise(sd_bus *bus, const char *path,
                                const char *interface, const char *property,
                                sd_bus_message *reply, void *userdata,
                                sd_bus_error *ret_error) {
  return sd_bus_message_append(reply, "b", false);
}

static int property_GetHasTrackList(sd_bus *bus, const char *path,
				    const char *interface, const char *property,
				    sd_bus_message *reply, void *userdata,
				    sd_bus_error *ret_error) {
  return sd_bus_message_append(reply, "b", false);
}

static int property_GetIdentity(sd_bus *bus, const char *path,
                                const char *interface, const char *property,
                                sd_bus_message *reply, void *userdata,
                                sd_bus_error *ret_error) {
  return sd_bus_message_append(reply, "s", "mus3 music player");
}

static int property_GetDesktopEntry(sd_bus *bus, const char *path,
				    const char *interface, const char *property,
				    sd_bus_message *reply, void *userdata,
				    sd_bus_error *ret_error) {
  return sd_bus_message_append(reply, "s", "mus3");
}

static int property_GetSupportedUriSchemes(sd_bus *bus, const char *path,
					   const char *interface, const char *property,
					   sd_bus_message *reply, void *userdata,
					   sd_bus_error *ret_error) {
  return sd_bus_message_append_strv(reply, (char*[]) { "file", NULL });
}

static int property_GetSupportedMimeTypes(sd_bus *bus, const char *path,
					  const char *interface, const char *property,
					  sd_bus_message *reply, void *userdata,
					  sd_bus_error *ret_error) {
  return sd_bus_message_append_strv(reply, (char*[]) { "audio/flac", "audio/mpeg", "audio/MPA", "audio/mpa-robust", NULL });
}

static int method_Raise(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  return sd_bus_error_setf(ret_error,
			   SD_BUS_ERROR_UNKNOWN_METHOD,
			   "Method '%s' is not supported",
			   "Raise");
}

static int method_Quit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  struct music_context *ctx = userdata;
  ctx->should_quit = true;
  return sd_bus_reply_method_return(m, "");
}


static const sd_bus_vtable mpris_vtable[] = {
  SD_BUS_VTABLE_START(0),

  SD_BUS_PROPERTY("CanQuit", "b", property_GetCanQuit, 0, 0),
  SD_BUS_WRITABLE_PROPERTY("Fullscreen", "b", property_GetFullscreen, property_SetFullscreen, 0, 0),
  SD_BUS_PROPERTY("CanSetFullscreen", "b", property_GetCanSetFullscreen, 0, 0),
  SD_BUS_PROPERTY("CanRaise", "b", property_GetCanRaise, 0, 0),
  SD_BUS_PROPERTY("HasTrackList", "b", property_GetHasTrackList, 0, 0),
  SD_BUS_PROPERTY("Identity", "s", property_GetIdentity, 0, 0),
  SD_BUS_PROPERTY("DesktopEntry", "s", property_GetDesktopEntry, 0, 0),
  SD_BUS_PROPERTY("SupportedUriSchemes", "as", property_GetSupportedUriSchemes, 0, 0),
  SD_BUS_PROPERTY("SupportedMimeTypes", "as", property_GetSupportedMimeTypes, 0, 0),

  SD_BUS_METHOD("Raise", "", "", method_Raise, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("Quit", "", "", method_Quit, SD_BUS_VTABLE_UNPRIVILEGED),
  
  SD_BUS_VTABLE_END
};

static const sd_bus_vtable mpris_player_vtable[] = {
  SD_BUS_VTABLE_START(0),

  SD_BUS_PROPERTY("PlaybackStatus", "s", property_GetPlaybackStatus, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_WRITABLE_PROPERTY("LoopStatus", "s", property_GetLoopStatus, property_SetLoopStatus, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_WRITABLE_PROPERTY("Rate", "d", property_GetRate, property_SetRate, 0, 0),
  SD_BUS_WRITABLE_PROPERTY("Shuffle", "b", property_GetShuffle, property_SetShuffle, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("Metadata", "a{sv}", property_GetMetadata, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_WRITABLE_PROPERTY("Volume", "d", property_GetVolume, property_SetVolume, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("Position", "x", property_GetPosition, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("MinimumRate", "d", property_GetMinimumRate, 0, 0),
  SD_BUS_PROPERTY("MaximumRate", "d", property_GetMaximumRate, 0, 0),
  SD_BUS_PROPERTY("CanGoNext", "b", property_GetCanGoNext, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanGoPrevious", "b", property_GetCanGoPrevious, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanPlay", "b", property_GetCanPlay, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanPause", "b", property_GetCanPause, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanSeek", "b", property_GetCanSeek, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
  SD_BUS_PROPERTY("CanControl", "b", property_GetCanControl, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),

  SD_BUS_SIGNAL("Seeked", "x", 0),
  
  SD_BUS_METHOD("Next", "", "", method_Next, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("Previous", "", "", method_Previous, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("Pause", "", "", method_Pause, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("PlayPause", "", "", method_PlayPause, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("Stop", "", "", method_Stop, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("Play", "", "", method_Play, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("Seek", "x", "", method_Seek, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("SetPosition", "ox", "", method_SetPosition, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("OpenUri", "s", "", method_OpenUri, SD_BUS_VTABLE_UNPRIVILEGED),

  SD_BUS_VTABLE_END
};

bool media_setup(struct music_context *ctx) {
  if (sd_bus_open_user(&bus) < 0) return false;
  if (sd_bus_request_name(bus, "org.mpris.MediaPlayer2.mus3", SD_BUS_NAME_ALLOW_REPLACEMENT | SD_BUS_NAME_REPLACE_EXISTING) < 0) return false;
  if (sd_bus_add_object_vtable(bus, &slot1, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2", mpris_vtable, ctx) < 0) return false;
  if (sd_bus_add_object_vtable(bus, &slot2, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", mpris_player_vtable, ctx) < 0) return false;
  
  return true;
}

enum media_event media_tick(void) {
  sd_bus_message *msg; if (sd_bus_process(bus, &msg) < 0) return EVENT_ERROR;

  sd_bus_wait(bus, 0);
  if (msg == NULL) return EVENT_NONE;
  
  return 69;
}

void media_update(enum media_changes changes) {
  if (bus == NULL) return;
  struct music_context *ctx = sd_bus_slot_get_userdata(slot1);
  char **properties_changed = da_new(ctx->arena, char*);
  if (changes & CHANGE_SEEK) {
    sd_bus_message *message = NULL;
    if (sd_bus_message_new_signal(bus, &message,
				  "/org/mpris/MediaPlayer2",
				  "org.mpris.MediaPlayer2.Player",
				  "Seeked") < 0) return;
    if (sd_bus_message_append(message, "x", (int64_t) (music_get_seek(ctx) * 1e6f)) < 0) return;
    if (sd_bus_message_send(message) < 0) return;
    da_push(&properties_changed, "Position");
  }
  if (changes & CHANGE_PLAYING) {
    da_push(&properties_changed, "PlaybackStatus");
    da_push(&properties_changed, "CanPlay");
    da_push(&properties_changed, "CanPause");
    da_push(&properties_changed, "CanSeek");
    da_push(&properties_changed, "CanControl");
    da_push(&properties_changed, "CanGoNext");
    da_push(&properties_changed, "CanGoPrevious");
  }
  if (changes & CHANGE_LOOP_MODE) {
    da_push(&properties_changed, "LoopStatus");
    da_push(&properties_changed, "Shuffle");
    da_push(&properties_changed, "CanGoNext");
    da_push(&properties_changed, "CanGoPrevious");
  }
  if (changes & CHANGE_VOLUME) {
    da_push(&properties_changed, "Volume");
  }
  if (changes & CHANGE_CURRENT_TRACK) {
    da_push(&properties_changed, "Metadata");
    da_push(&properties_changed, "PlaybackStatus");
    da_push(&properties_changed, "CanGoNext");
    da_push(&properties_changed, "CanGoPrevious");
  }
  da_push(&properties_changed, NULL);
  if (sd_bus_emit_properties_changed_strv(bus, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", properties_changed) < 0) return;
}

void media_destroy(void) {
  if (bus) {
    sd_bus_slot_unref(slot1); slot1 = NULL;
    sd_bus_slot_unref(slot2); slot2 = NULL;
    sd_bus_unref(bus); bus = NULL;
  }
}
