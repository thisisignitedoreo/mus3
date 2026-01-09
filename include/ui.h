#pragma once

#include <allocator.h>

#include "music.h"

#include <raylib.h>
#include <raytext.h>

struct ui_theme {
  const char *name, *author;
  Color background, background_p1, background_p2;
  Color foreground_m2, foreground_m1, foreground;
  Color middleground, middleground_p1;
  Color link;
};

enum ui_cursor_state {
  UI_DEFAULT,
  UI_CLICK,
  UI_SCROLL,
  UI_TYPE,
};

enum ui_page {
  UI_PLAYLIST,
  UI_ALBUMS,
  UI_BROWSE,
  UI_SETTINGS,
  UI_ABOUT,
  UI_ALBUM,
  UI_PAGES,
};

struct ui_context {
  struct allocator arena;
  
  struct {
    bool dont_load_covers;
  } settings;
  
  float fontsize;
  RtFont font;

  enum ui_page page;
  enum ui_cursor_state cursor;

  int current_album;

  float scroll[UI_PAGES];

  struct {
    Texture play, pause;
    Texture previous, next;
    Texture arrow_up, arrow_down;
    struct {
      Texture linear, playlist, one, shuffle;
    } repeat;
    Texture check;
  } icons;

  struct {
    bool seeking;
    bool pause_after_seeking;
  } pc;

  struct {
    float volume_hint;
  } sidebar;

  struct {
    bool open;
    Rectangle rect;
    int track;
  } rmb_menu;

  struct {
    bool open;
    Rectangle rect;
  } dropdown;
  
  bool debug;

  Texture *texture_pool;
  Texture empty_cover;
  bool cover_loaded_this_frame;

  uint8_t *cover_buffer;
  size_t cover_buffer_size;

  bool search_focused;
  char *search;

  struct {
    char *inputbox;
    bool focused;
  } add;

#define UI_STR_LENGTH 512
#define UI_RESULTS 64
#define UI_TOKENS 64
  struct ui_search_ctx {
    struct ui_album_song {
      bool is_album;
      union {
	struct music_album album;
	struct music_song song;
      } v;
    } results[UI_RESULTS];
    int result_count;
    char *normalized_query[UI_STR_LENGTH];
    char normalized_query_s[UI_STR_LENGTH][UI_TOKENS];
    struct ui_pair {
      size_t id;
      int value;
    } *result_ids;
  } *search_ctx;

  double last_keystroke;
  
  struct ui_theme *themes;
  int theme_idx;
};

bool setup_ui(struct allocator arena, struct ui_context *ctx);
void destroy_ui(struct ui_context *ctx);
void draw_ui(struct ui_context *ctx, struct music_context *music_ctx);
bool ui_done(struct ui_context *ctx);
