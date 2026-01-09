#include <math.h>
#include <string.h>
#include <dynamic-array.h>
#include <arena.h>

#include <stdio.h>
#include <stdlib.h>

#define STB_TRUETYPE_IMPLEMENTATION

#include "ui.h"
#include "stuff.h"

#include "assets.h"

#define CURRENT_THEME(c) (&(c)->themes[(c)->theme_idx])
#define SIZE(a) (sizeof(a)/sizeof(*a))

Texture load_icon(const unsigned char *data, size_t size, float fontsize) {
  Image image = LoadImageFromMemory(".png", data, size);
  ImageResize(&image, fontsize, fontsize);
  Texture icon = LoadTextureFromImage(image);
  UnloadImage(image);
  return icon;
}

bool setup_ui(struct allocator arena, struct ui_context *ctx) {
  memset(ctx, 0, sizeof(*ctx));

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
  InitWindow(800, 600, "mus3");
  SetExitKey(0);
  
  ctx->arena = arena;
  ctx->fontsize = 24.0f * GetWindowScaleDPI().y;
  SetWindowMinSize(ctx->fontsize * 24.0f, ctx->fontsize * 13.5f);
  ctx->font = LoadRtFontFromMemory(_FONT, _FONT_LENGTH);
  ctx->texture_pool = da_new(arena, Texture);
  ctx->add.inputbox = da_new(arena, char);
  ctx->add.focused = false;
  ctx->search = da_new(arena, char);
  ctx->search_ctx = a_malloc(stdc_allocator(), sizeof(ctx->search_ctx[0]));
  ctx->search_ctx->result_ids = da_new(stdc_allocator(), struct ui_pair);
  ctx->themes = da_new(arena, struct ui_theme);
  struct ui_theme default_theme = {
    .name = "Default dark theme",
    .author = "mus3 developers",
    .background = GetColor(0x181818FF),
    .background_p1 = GetColor(0x282828FF),
    .background_p2 = GetColor(0x453d41FF),
    .middleground = GetColor(0x453d41FF),
    .middleground_p1 = GetColor(0x62595eFF),
    .foreground_m2 = GetColor(0x82797eFF),
    .foreground_m1 = GetColor(0xbfb6baFF),
    .foreground = GetColor(0xe4e4efFF),
    .link = GetColor(0x96a6c8FF),
  };
  da_push(&ctx->themes, default_theme);
  struct ui_theme not_default_theme = {
    .name = "gruvbox",
    .author = "morhertz",
    .background = GetColor(0x282828FF),
    .background_p1 = GetColor(0x3c3836FF),
    .background_p2 = GetColor(0x504945FF),
    .middleground = GetColor(0x504945FF),
    .middleground_p1 = GetColor(0x665c54FF),
    .foreground_m2 = GetColor(0xa89984FF),
    .foreground_m1 = GetColor(0xd5c4a1FF),
    .foreground = GetColor(0xeddbb2FF),
    .link = GetColor(0x689d6aFF),
  };
  da_push(&ctx->themes, not_default_theme);
  not_default_theme = (struct ui_theme) {
    .name = "purpl",
    .author = "me",
    .background = GetColor(0x3D365CFF),
    .background_p1 = GetColor(0x453E64FF),
    .background_p2 = GetColor(0x4D466CFF),
    .middleground = GetColor(0x4D466CFF),
    .middleground_p1 = GetColor(0x5D567CFF),
    .foreground_m2 = GetColor(0x7D769CFF),
    .foreground_m1 = GetColor(0xD8953FFF),
    .foreground = GetColor(0xF8B55FFF),
    .link = GetColor(0xC95792FF),
  };
  da_push(&ctx->themes, not_default_theme);
  
  Image empty_cover = GenImageColor(ctx->fontsize * 6.0f, ctx->fontsize * 6.0f, CURRENT_THEME(ctx)->background_p2);
  ctx->empty_cover = LoadTextureFromImage(empty_cover);
  UnloadImage(empty_cover);
  
  TraceLog(LOG_INFO, "Initialized UI successfully");
  
  return false;
}

void destroy_ui(struct ui_context *ctx) {
  da_free(&ctx->search_ctx->result_ids);
  a_free(stdc_allocator(), ctx->search_ctx);
  
  for (size_t i = 0; i < da_size(&ctx->texture_pool); i++) {
    UnloadTexture(ctx->texture_pool[i]);
  }
  UnloadTexture(ctx->empty_cover);

  if (ctx->cover_buffer) a_free(stdc_allocator(), ctx->cover_buffer);
  
  UnloadRtFont(&ctx->font);
  UnloadTexture(ctx->icons.next);
  UnloadTexture(ctx->icons.previous);
  UnloadTexture(ctx->icons.play);
  UnloadTexture(ctx->icons.pause);
  UnloadTexture(ctx->icons.arrow_up);
  UnloadTexture(ctx->icons.arrow_down);
  UnloadTexture(ctx->icons.repeat.linear);
  UnloadTexture(ctx->icons.repeat.playlist);
  UnloadTexture(ctx->icons.repeat.one);
  UnloadTexture(ctx->icons.repeat.shuffle);
  UnloadTexture(ctx->icons.check);

  CloseWindow();
}

void load_texture(struct ui_context *ctx) {
  if (ctx->icons.next.id == 0) ctx->icons.next = load_icon(_NEXT, _NEXT_LENGTH, ctx->fontsize);
  else if (ctx->icons.previous.id == 0) ctx->icons.previous = load_icon(_PREVIOUS, _PREVIOUS_LENGTH, ctx->fontsize);
  else if (ctx->icons.play.id == 0) ctx->icons.play = load_icon(_PLAY, _PLAY_LENGTH, ctx->fontsize);
  else if (ctx->icons.pause.id == 0) ctx->icons.pause = load_icon(_PAUSE, _PAUSE_LENGTH, ctx->fontsize);
  else if (ctx->icons.arrow_up.id == 0) ctx->icons.arrow_up = load_icon(_ARROW_UP, _ARROW_UP_LENGTH, ctx->fontsize);
  else if (ctx->icons.arrow_down.id == 0) ctx->icons.arrow_down = load_icon(_ARROW_DOWN, _ARROW_DOWN_LENGTH, ctx->fontsize);
  else if (ctx->icons.repeat.linear.id == 0) ctx->icons.repeat.linear = load_icon(_REPEAT_LINEAR, _REPEAT_LINEAR_LENGTH, ctx->fontsize);
  else if (ctx->icons.repeat.playlist.id == 0) ctx->icons.repeat.playlist = load_icon(_REPEAT_ALBUM, _REPEAT_ALBUM_LENGTH, ctx->fontsize);
  else if (ctx->icons.repeat.one.id == 0) ctx->icons.repeat.one = load_icon(_REPEAT_ONE, _REPEAT_ONE_LENGTH, ctx->fontsize);
  else if (ctx->icons.repeat.shuffle.id == 0) ctx->icons.repeat.shuffle = load_icon(_REPEAT_SHUFFLE, _REPEAT_SHUFFLE_LENGTH, ctx->fontsize);
  else if (ctx->icons.check.id == 0) ctx->icons.check = load_icon(_CHECK, _CHECK_LENGTH, ctx->fontsize);
}

float get_text_width(struct ui_context *ctx, const char *text) {
  return MeasureRtTextEx(&ctx->font, text, ctx->fontsize, 0).x;
}

float get_max_width(struct ui_context *ctx, const char **lines) {
  float max = 0.0f;
  for (const char **line = lines; *line; line++) {
    float width = get_text_width(ctx, *line);
    if (width > max) max = width;
  }
  return max;
}

Texture get_cover(struct ui_context *ctx, struct music_cover *cover) {
  if (ctx->settings.dont_load_covers || !cover->exists || ctx->cover_loaded_this_frame) return ctx->empty_cover;
  if (cover->texture >= 0) return ctx->texture_pool[cover->texture];
  struct allocator alloc = stdc_allocator();
  ctx->cover_loaded_this_frame = true;

  Texture cover_texture = ctx->empty_cover;
  
  FILE *f = fopen(cover->path, "rb");
  if (f == NULL) goto return_cover;

  fseek(f, cover->offset, SEEK_SET);

  if (ctx->cover_buffer_size < cover->size) {
    ctx->cover_buffer = a_realloc(alloc, ctx->cover_buffer, cover->size);
    if (ctx->cover_buffer == NULL) goto close_file;
  }
  
  if (!fread(ctx->cover_buffer, cover->size, 1, f)) goto close_file;

  Image image = LoadImageFromMemory(cover->ext, ctx->cover_buffer, cover->size);
  ImageResize(&image, ctx->fontsize * 6.0f, ctx->fontsize * 6.0f);
  cover_texture = LoadTextureFromImage(image);

  cover->texture = da_size(&ctx->texture_pool);
  da_push(&ctx->texture_pool, cover_texture);

 close_file:
  fclose(f);
 return_cover:
  return cover_texture;
}

const char *sidebar_items[] = {
  "mus3",
  "Playlist",
  "Albums",
  "Browse",
  "Settings",
  "About",
  "Scanning...",
  "Volume: 100%",
  NULL
};

#define mouse_in_rmb_menu(ctx) ((ctx)->rmb_menu.open && (CheckCollisionPointRec(GetMousePosition(), (ctx)->rmb_menu.rect)))
#define mouse_in_dropdown(ctx) ((ctx)->dropdown.open && (CheckCollisionPointRec(GetMousePosition(), (ctx)->dropdown.rect)))
#define mouse_in_rect(ctx, rect) (IsCursorOnScreen() && !mouse_in_rmb_menu(ctx) && !mouse_in_dropdown(ctx) && CheckCollisionPointRec(GetMousePosition(), (rect)))
#define mouse_in_2rect(ctx, window, rect) (mouse_in_rect(ctx, rect) && CheckCollisionPointRec(GetMousePosition(), (window)))

#define key_pressed(ctx, key) (!(ctx)->search_focused && !(ctx)->add.focused && IsKeyPressed((key)))

static inline const char *seconds_to_text(float seconds) {
  if (seconds > 60*60) TextFormat("%d:%02d:%02d", (int) seconds / 3600, ((int) seconds / 60) % 60, (int) seconds % 60);
  return TextFormat("%d:%02d", (int) seconds / 60, (int) seconds % 60);
}

void draw_sidebar(struct ui_context *ctx, struct music_context *music_ctx) {
  Rectangle sidebar = { 0, 0, get_max_width(ctx, sidebar_items) + ctx->fontsize * 1.0f, GetScreenHeight() - ctx->fontsize * 3.5f };
  BeginScissorMode(sidebar.x, sidebar.y, sidebar.width, sidebar.height);
  ClearBackground(CURRENT_THEME(ctx)->background);

  DrawRtTextEx(&ctx->font, sidebar_items[0], (Vector2) { ctx->fontsize * 0.5f, ctx->fontsize * 0.5f }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);

  for (int i = 1; i < 6; i++) {
    float y = ctx->fontsize * (1.0f + i);
    Rectangle button = { 0, y, sidebar.width, ctx->fontsize };
    if (mouse_in_rect(ctx, button)) ctx->cursor = UI_CLICK;
    if (mouse_in_rect(ctx, button) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ctx->page = (enum ui_page) (i - 1);
    if (mouse_in_rect(ctx, button) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      DrawRectangleRec(button, CURRENT_THEME(ctx)->foreground);
      DrawRtTextEx(&ctx->font, sidebar_items[i], (Vector2) { ctx->fontsize * 0.5f, y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->background);
    } else if (mouse_in_rect(ctx, button) || ctx->page == (enum ui_page) (i - 1)) {
      DrawRectangleRec(button, CURRENT_THEME(ctx)->background_p1);
      DrawRtTextEx(&ctx->font, sidebar_items[i], (Vector2) { ctx->fontsize * 0.5f, y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    } else {
      DrawRtTextEx(&ctx->font, sidebar_items[i], (Vector2) { ctx->fontsize * 0.5f, y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    }
  }

  if (music_is_load_queued(music_ctx)) DrawRtTextEx(&ctx->font, "Scanning...", (Vector2) { ctx->fontsize * 0.5f, sidebar.height - ctx->fontsize * 2.5f }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
  
  Rectangle button = { 0, sidebar.height - ctx->fontsize * 1.5f, sidebar.width, ctx->fontsize };
  float volume = music_get_volume(music_ctx);
  if (mouse_in_rect(ctx, button)) {
    ctx->cursor = UI_SCROLL;
    DrawRectangleRec(button, CURRENT_THEME(ctx)->background_p1);
    
    float scroll = GetMouseWheelMoveV().y;
    if (scroll < 0.0f) music_set_volume(music_ctx, volume > 0.05f ? volume - 0.05f : 0.0f);
    if (scroll > 0.0f) music_set_volume(music_ctx, volume < 0.95f ? volume + 0.05f : 1.0f);
  }

  DrawRtTextEx(&ctx->font, TextFormat("Volume: %.0f%%", volume * 100.0f), (Vector2) { ctx->fontsize * 0.5f, button.y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
  
  EndScissorMode();
}


void draw_scrollbar(struct ui_context *ctx, Rectangle main, float scroll, float content_height) {
  float real_height = content_height - main.height;
  float scrollbar_size = main.height / content_height;
  if (scrollbar_size >= 1.0f) return;
  float real_size = ceilf(scrollbar_size * main.height);
  if (real_size < ctx->fontsize * 0.5f) real_size = ctx->fontsize * 0.5f;
  float width = roundf(ctx->fontsize * 0.225f);
  DrawRectangle(main.x + main.width - width, main.y, width, main.height, CURRENT_THEME(ctx)->middleground);
  DrawRectangle(main.x + main.width - width, main.y + (scroll / real_height) * (main.height - real_size), width, real_size, CURRENT_THEME(ctx)->middleground_p1);
}

void handle_scroll(struct ui_context *ctx, Rectangle main, float *scroll, float content_height) {
  float real_height = content_height - main.height;
  
  if (real_height < *scroll) *scroll = real_height;
  if (*scroll < 0) *scroll = 0;

  if (!CheckCollisionPointRec(GetMousePosition(), main)) return;
  
  float scroll_factor = ctx->fontsize * 2.0f;
  float mscroll = GetMouseWheelMoveV().y;
  if (mscroll < 0.0f) {
    *scroll += scroll_factor;
    if (real_height < *scroll) *scroll = real_height;
    if (*scroll < 0) *scroll = 0;
  } else if (mscroll > 0.0f) {
    *scroll -= scroll_factor;
    if (real_height < *scroll) *scroll = real_height;
    if (*scroll < 0) *scroll = 0;
  }
}

int draw_rmb_menu(struct ui_context *ctx, const char **menu) {
  int clicked = -1;
  ClearBackground(CURRENT_THEME(ctx)->background);
  float dy = 0.0f;
  for (int i = 0; menu[i]; i++) {
    Rectangle hitbox = { ctx->rmb_menu.rect.x, ctx->rmb_menu.rect.y + dy, ctx->rmb_menu.rect.width, ctx->fontsize };
    bool hovered = IsCursorOnScreen() && mouse_in_rmb_menu(ctx) && CheckCollisionPointRec(GetMousePosition(), hitbox);
    if (hovered) {
      DrawRectangleRec(hitbox, CURRENT_THEME(ctx)->background_p1);
      ctx->cursor = UI_CLICK;
    }
    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = i;
    DrawRtTextEx(&ctx->font, menu[i], (Vector2) { ctx->rmb_menu.rect.x + ctx->fontsize * 0.5f, ctx->rmb_menu.rect.y + dy }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    if (i == 0 || i == 1) DrawRtTextEx(&ctx->font, i == 0 ? "LMB" : "Shift + LMB",
				     (Vector2) {
				       ctx->rmb_menu.rect.x + ctx->rmb_menu.rect.width - MeasureRtTextEx(&ctx->font, i == 0 ? "LMB" : "Shift + LMB", ctx->fontsize, 0).x - ctx->fontsize * 0.5f,
				       ctx->rmb_menu.rect.y + dy }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
    dy += ctx->fontsize;
  }
  if (clicked >= 0) ctx->rmb_menu.open = false;
  return clicked;
}

int draw_dropdown_themes(struct ui_context *ctx) {
  int clicked = -1;
  ClearBackground(CURRENT_THEME(ctx)->background);
  float dy = 0.0f;
  for (size_t i = 0; i < da_size(&ctx->themes); i++) {
    Rectangle hitbox = { ctx->dropdown.rect.x, ctx->dropdown.rect.y + dy, ctx->dropdown.rect.width, ctx->fontsize };
    bool hovered = IsCursorOnScreen() && mouse_in_dropdown(ctx) && CheckCollisionPointRec(GetMousePosition(), hitbox);
    if (hovered) {
      DrawRectangleRec(hitbox, CURRENT_THEME(ctx)->background_p1);
      ctx->cursor = UI_CLICK;
    }
    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = i;
    DrawRtTextEx(&ctx->font, ctx->themes[i].name, (Vector2) { ctx->dropdown.rect.x + ctx->fontsize * 0.5f, ctx->dropdown.rect.y + dy }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    dy += ctx->fontsize;
  }
  if (clicked >= 0) ctx->dropdown.open = false;
  return clicked;
}

void draw_separator(struct ui_context *ctx, Rectangle main, float y, const char *text) {
  int thick = ctx->fontsize / 12.0f;
  DrawRectangle(main.x + ctx->fontsize * 0.5f, y + ctx->fontsize * 0.5f - thick * 0.5f, ctx->fontsize, thick, CURRENT_THEME(ctx)->background_p2);
  DrawRtTextEx(&ctx->font, text, (Vector2) { main.x + ctx->fontsize * 2.0f, y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
  float tx = ctx->fontsize * 2.5f + MeasureRtTextEx(&ctx->font, text, ctx->fontsize, 0).x;
  DrawRectangle(main.x + tx, y + ctx->fontsize * 0.5f - thick * 0.5f, main.width - tx - ctx->fontsize * 0.5f, thick, CURRENT_THEME(ctx)->background_p2);
}

bool draw_button(struct ui_context *ctx, Rectangle main, Vector2 pos, const char *text) {
  Rectangle hitbox = { pos.x, pos.y, ctx->fontsize * 0.5f + MeasureRtTextEx(&ctx->font, text, ctx->fontsize, 0).x, ctx->fontsize };
  bool hovered = mouse_in_2rect(ctx, main, hitbox);
  if (hovered) ctx->cursor = UI_CLICK;
  if (hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    DrawRectangleRec(hitbox, CURRENT_THEME(ctx)->foreground);
    DrawRtTextEx(&ctx->font, text, (Vector2) { pos.x + ctx->fontsize * 0.25f, pos.y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->background_p1);
  } else if (hovered) {
    DrawRectangleRec(hitbox, CURRENT_THEME(ctx)->middleground_p1);
    DrawRtTextEx(&ctx->font, text, (Vector2) { pos.x + ctx->fontsize * 0.25f, pos.y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
  } else {
    DrawRectangleRec(hitbox, CURRENT_THEME(ctx)->middleground);
    DrawRtTextEx(&ctx->font, text, (Vector2) { pos.x + ctx->fontsize * 0.25f, pos.y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
  }
  return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

Rectangle rect_overlap(Rectangle a, Rectangle b) {
  if (a.x < b.x) a.x = b.x;
  if (a.x + a.width > b.x + b.width) a.width = b.x + b.width - a.x;
  if (a.y < b.y) a.y = b.y;
  if (a.y + a.height > b.y + b.height) a.height = b.y + b.height - a.y;
  return a;
}

void set_theme(struct ui_context *ctx, int theme_idx) {
  ctx->theme_idx = theme_idx;
  UnloadTexture(ctx->empty_cover);
  Image empty_cover = GenImageColor(ctx->fontsize * 6.0f, ctx->fontsize * 6.0f, CURRENT_THEME(ctx)->background_p2);
  ctx->empty_cover = LoadTextureFromImage(empty_cover);
  UnloadImage(empty_cover);
}

void draw_main(struct ui_context *ctx, struct music_context *music_ctx) {
  Rectangle main = { get_max_width(ctx, sidebar_items) + ctx->fontsize * 1.0f, 0, ceilf(GetScreenWidth() - (get_max_width(ctx, sidebar_items) + ctx->fontsize * 1.0f)), GetScreenHeight() - ctx->fontsize * 3.5f };
  BeginScissorMode(main.x, main.y, main.width, main.height);
  ClearBackground(CURRENT_THEME(ctx)->background_p1);

  Vector2 mouse = GetMousePosition();

  float dx = floorf(main.x), dy = floorf(-ctx->scroll[ctx->page] + main.y), h = 0.0f;
  if (ctx->page == UI_PLAYLIST) {
    float y = 0.5f * ctx->fontsize, x = 0.5f * ctx->fontsize;
    if (da_size(&music_ctx->playlist) == 0) DrawRtTextEx(&ctx->font, "*crickets*", (Vector2) {dx + x, dy + y}, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
    
    Texture cover = {0};
    if (music_ctx->current_song_idx >= 0) {
      cover = get_cover(ctx, &music_ctx->song_cache[music_ctx->current_song_idx].cover);
    }

    Rectangle cover_texture = { main.x + main.width - ctx->fontsize * 6.5f, main.y + main.height - ctx->fontsize * 6.5f, ctx->fontsize * 6.0f, ctx->fontsize * 6.0f };
    
    const char *last_album_title = NULL, *last_album_artist = NULL;
    float line_length = 0.0f;
    for (size_t i = 0; i < da_size(&music_ctx->playlist); i++) {
      const char *tmp = TextFormat("%d. ", i+1);
      float width = MeasureRtTextEx(&ctx->font, tmp, ctx->fontsize, 0).x;
      if (width > line_length) line_length = width;
    }
    
    for (size_t i = 0; i < da_size(&music_ctx->playlist); i++) {
      struct music_song song = music_ctx->playlist[i];
      bool no_tags = song.title == NULL;
      if (song.title		== NULL) song.title		= "<null>";
      if (song.artist		== NULL) song.artist		= "<null>";
      if (song.album.title	== NULL) song.album.title	= "<null>";
      if (song.album.artist	== NULL) song.album.artist	= "<null>";

      x = ctx->fontsize;
      if (!no_tags) {
	if (!last_album_title || !last_album_artist || strcmp(song.album.title, last_album_title) || strcmp(song.album.artist, last_album_artist)) {
	  x = ctx->fontsize * 0.5f;
	  DrawRtTextEx(&ctx->font, song.album.artist, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
	  x += MeasureRtTextEx(&ctx->font, song.album.artist, ctx->fontsize, 0).x;
	  DrawRtTextEx(&ctx->font, " - ", (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
	  x += MeasureRtTextEx(&ctx->font, " - ", ctx->fontsize, 0).x;
	  DrawRtTextEx(&ctx->font, song.album.title, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
	  x += MeasureRtTextEx(&ctx->font, song.album.title, ctx->fontsize, 0).x;
	  DrawRtTextEx(&ctx->font, TextFormat(" (%d)", song.year), (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
	  x = ctx->fontsize;
	  y += ctx->fontsize;
	}

	last_album_title = song.album.title;
	last_album_artist = song.album.artist;
      }

      bool hovered = mouse_in_2rect(ctx, main, ((Rectangle) { dx, dy + y, main.width, ctx->fontsize })) && \
	(cover.id == ctx->empty_cover.id || !CheckCollisionPointRec(mouse, cover_texture)) && !mouse_in_rmb_menu(ctx);
      if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	if (IsKeyDown(KEY_LEFT_SHIFT)) music_playlist_pop(music_ctx, i);
	else music_load(music_ctx, song.path);
      }
      if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
	ctx->rmb_menu.open = true;
	ctx->rmb_menu.rect = (Rectangle) { mouse.x, mouse.y, MeasureRtTextEx(&ctx->font, "DeleteShift + LMB", ctx->fontsize, 0).x + ctx->fontsize * 1.5f, ctx->fontsize * 4.0f };
	ctx->rmb_menu.track = i;

	if (ctx->rmb_menu.rect.x + ctx->rmb_menu.rect.width >= GetScreenWidth()) ctx->rmb_menu.rect.x -= ctx->rmb_menu.rect.width;
	if (ctx->rmb_menu.rect.y + ctx->rmb_menu.rect.height >= GetScreenHeight()) ctx->rmb_menu.rect.y -= ctx->rmb_menu.rect.height;

	ctx->rmb_menu.rect.x = floorf(ctx->rmb_menu.rect.x);
	ctx->rmb_menu.rect.y = floorf(ctx->rmb_menu.rect.y);
	ctx->rmb_menu.rect.width = floorf(ctx->rmb_menu.rect.width);
	ctx->rmb_menu.rect.height = floorf(ctx->rmb_menu.rect.height);
     }

      if (hovered) {
	DrawRectangle(dx, dy + y, main.width, ctx->fontsize, CURRENT_THEME(ctx)->background_p2);
	ctx->cursor = UI_CLICK;
      }

      const char *tmp = TextFormat("%d. ", i+1);
      bool this_song = music_ctx->current_song_idx >= 0 && paths_equal(music_ctx->song_cache[music_ctx->current_song_idx].path, song.path);
      Color dark = this_song ? CURRENT_THEME(ctx)->foreground_m1 : CURRENT_THEME(ctx)->foreground_m2;
      DrawRtTextEx(&ctx->font, tmp, (Vector2) { dx + x + line_length - MeasureRtTextEx(&ctx->font, tmp, ctx->fontsize, 0).x, dy + y }, ctx->fontsize, 0, dark);
      x += line_length;
      if (no_tags) {
	DrawRtTextEx(&ctx->font, basename((char*) song.path), (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
      } else {
	DrawRtTextEx(&ctx->font, song.artist, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
	x += MeasureRtTextEx(&ctx->font, song.artist, ctx->fontsize, 0).x;
	DrawRtTextEx(&ctx->font, " - ", (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, dark);
	x += MeasureRtTextEx(&ctx->font, " - ", ctx->fontsize, 0).x;
	DrawRtTextEx(&ctx->font, song.title, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
	x += MeasureRtTextEx(&ctx->font, song.title, ctx->fontsize, 0).x;
      }
      y += ctx->fontsize;
    }
    
    if (cover.id != ctx->empty_cover.id) DrawTexture(cover, cover_texture.x, cover_texture.y, WHITE);
    
    if (ctx->rmb_menu.open) {
      EndScissorMode();
      BeginScissorMode(ctx->rmb_menu.rect.x, ctx->rmb_menu.rect.y, ctx->rmb_menu.rect.width, ctx->rmb_menu.rect.height);

      const char *menu[] = {
	"Play", "Delete",
	"Move up", "Move down",
	NULL
      };

      switch (draw_rmb_menu(ctx, menu)) {
      case 0: music_load(music_ctx, music_ctx->playlist[ctx->rmb_menu.track].path); break;
      case 1: music_playlist_pop(music_ctx, ctx->rmb_menu.track); break;
      case 2: {
	if (ctx->rmb_menu.track == 0) break;
	struct music_song temp = music_ctx->playlist[ctx->rmb_menu.track];
	music_ctx->playlist[ctx->rmb_menu.track] = music_ctx->playlist[ctx->rmb_menu.track - 1];
	music_ctx->playlist[ctx->rmb_menu.track - 1] = temp;
      } break;
      case 3: {
	if ((size_t) ctx->rmb_menu.track == da_size(&music_ctx->playlist) - 1) break;
	struct music_song temp = music_ctx->playlist[ctx->rmb_menu.track];
	music_ctx->playlist[ctx->rmb_menu.track] = music_ctx->playlist[ctx->rmb_menu.track + 1];
	music_ctx->playlist[ctx->rmb_menu.track + 1] = temp;
      } break;
      }
      
      EndScissorMode();
      BeginScissorMode(main.x, main.y, main.width, main.height);
    }

    if (music_ctx->current_song_idx < 0 && da_size(&music_ctx->playlist) && IsKeyPressed(KEY_ENTER)) {
      music_load(music_ctx, music_ctx->playlist[0].path);
    }
    
    y += ctx->fontsize * 0.5f; h = y;
  } else if (ctx->page == UI_ALBUM) {
    struct music_album album = music_ctx->albums[ctx->current_album];

    if (!ctx->settings.dont_load_covers) DrawTexture(get_cover(ctx, &music_ctx->albums[ctx->current_album].cover), dx + ctx->fontsize * 0.5f, dy + ctx->fontsize * 0.5f, WHITE);
    
    float x = ctx->fontsize * (ctx->settings.dont_load_covers ? 0.5f : 7.0f), y = ctx->fontsize * 0.5f;
    
    DrawRtTextEx(&ctx->font, album.title, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    x += MeasureRtTextEx(&ctx->font, album.title, ctx->fontsize, 0).x;
    DrawRtTextEx(&ctx->font, TextFormat(" (%d)", album.year), (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
    x = ctx->fontsize * (ctx->settings.dont_load_covers ? 0.5f : 7.0f); y += ctx->fontsize;
    DrawRtTextEx(&ctx->font, album.artist, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    y += ctx->fontsize;
    DrawRtTextEx(&ctx->font, seconds_to_text(album.length / 1000), (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);

    float line_length = 0;
    for (size_t i = 0; i < da_size(&album.tracklist); i++) {
      const char *tmp = TextFormat("%d. ", album.tracklist[i].track.this);
      float width = MeasureRtTextEx(&ctx->font, tmp, ctx->fontsize, 0).x;
      if (width > line_length) line_length = width;
    }

    y = ctx->fontsize * (ctx->settings.dont_load_covers ? 4.0f : 7.0f); x = ctx->fontsize * 0.5f;
    for (size_t i = 0; i < da_size(&album.tracklist); i++) {
      struct music_song track = album.tracklist[i];

      bool hovered = mouse_in_2rect(ctx, main, ((Rectangle) { dx, dy + y, main.width, ctx->fontsize }));
      if (hovered) {
	DrawRectangle(dx, dy + y, main.width, ctx->fontsize, CURRENT_THEME(ctx)->background_p2);
	ctx->cursor = UI_CLICK;
      }

      if (hovered && IsKeyDown(KEY_LEFT_SHIFT) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	music_playlist_push(music_ctx, track.path);
      } else if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        music_unload(music_ctx);
	da_size(&music_ctx->playlist) = 0;
	for (size_t i = 0; i < da_size(&album.tracklist); i++) music_playlist_push(music_ctx, album.tracklist[i].path);
	music_load(music_ctx, track.path);
      }
      
      if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
	ctx->rmb_menu.open = true;
	ctx->rmb_menu.rect = (Rectangle) { mouse.x, mouse.y, MeasureRtTextEx(&ctx->font, "Add to playlistShift + LMB", ctx->fontsize, 0).x + ctx->fontsize * 1.5f, ctx->fontsize * 3.0f };
	ctx->rmb_menu.track = i;

	if (ctx->rmb_menu.rect.x + ctx->rmb_menu.rect.width >= GetScreenWidth()) ctx->rmb_menu.rect.x -= ctx->rmb_menu.rect.width;
	if (ctx->rmb_menu.rect.y + ctx->rmb_menu.rect.height >= GetScreenHeight()) ctx->rmb_menu.rect.y -= ctx->rmb_menu.rect.height;
        
	ctx->rmb_menu.rect.x = floorf(ctx->rmb_menu.rect.x);
	ctx->rmb_menu.rect.y = floorf(ctx->rmb_menu.rect.y);
	ctx->rmb_menu.rect.width = floorf(ctx->rmb_menu.rect.width);
	ctx->rmb_menu.rect.height = floorf(ctx->rmb_menu.rect.height);
      }

      const char *tmp = TextFormat("%d. ", album.tracklist[i].track.this);
      x += line_length;
      DrawRtTextEx(&ctx->font, tmp, (Vector2) { dx + x - MeasureRtTextEx(&ctx->font, tmp, ctx->fontsize, 0).x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);

      DrawRtTextEx(&ctx->font, track.artist, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
      x += MeasureRtTextEx(&ctx->font, track.artist, ctx->fontsize, 0).x;
      DrawRtTextEx(&ctx->font, " - ", (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
      x += MeasureRtTextEx(&ctx->font, " - ", ctx->fontsize, 0).x;
      DrawRtTextEx(&ctx->font, track.title, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
      
      y += ctx->fontsize * 1.0f;
      x = ctx->fontsize * 0.5f;
    }

    if (ctx->rmb_menu.open) {
      EndScissorMode();
      BeginScissorMode(ctx->rmb_menu.rect.x, ctx->rmb_menu.rect.y, ctx->rmb_menu.rect.width, ctx->rmb_menu.rect.height);

      const char *menu[] = {
	"Play", "Add to playlist",
	"Add album to playlist",
	NULL
      };

      switch (draw_rmb_menu(ctx, menu)) {
      case 0: {
        music_unload(music_ctx);
	da_size(&music_ctx->playlist) = 0;
	for (size_t i = 0; i < da_size(&album.tracklist); i++) music_playlist_push(music_ctx, album.tracklist[i].path);
	music_load(music_ctx, album.tracklist[ctx->rmb_menu.track].path);
      } break;
      case 1: music_playlist_push(music_ctx, album.tracklist[ctx->rmb_menu.track].path); break;
      case 2: {
	for (size_t i = 0; i < da_size(&album.tracklist); i++) music_playlist_push(music_ctx, album.tracklist[i].path);
      } break;
      }
      
      EndScissorMode();
      BeginScissorMode(main.x, main.y, main.width, main.height);
    }

    h = y + ctx->fontsize * 0.5f;
  } else if (ctx->page == UI_BROWSE) {
    da_push(&ctx->search, 0);

    Rectangle hitbox = { dx, dy, main.width, ctx->fontsize * 2.0f };
    DrawRectangleRec(hitbox, CURRENT_THEME(ctx)->background);
    
    if (da_size(&ctx->search) == 1) {
      if (ctx->search_focused) {
	Color color = CURRENT_THEME(ctx)->foreground_m1;
	color.a = (unsigned char) ((cos((GetTime() - ctx->last_keystroke) * M_PI * 0.5f) + 1.0f) / 2.0f * color.a);
	DrawRectangle(dx + ctx->fontsize * 0.5f, dy + ctx->fontsize * 0.5f, ctx->fontsize / 12.0f, ctx->fontsize, color);
      } else {
	DrawRtTextEx(&ctx->font, "Click here & start typing!", (Vector2) { dx + ctx->fontsize * 0.5f, dy + ctx->fontsize * 0.5f }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
      }
    } else {
      DrawRtTextEx(&ctx->font, ctx->search, (Vector2) { dx + ctx->fontsize * 0.5f, dy + ctx->fontsize * 0.5f }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
      if (ctx->search_focused) {
	Color color = CURRENT_THEME(ctx)->foreground_m1;
	color.a = (unsigned char) ((cos((GetTime() - ctx->last_keystroke) * M_PI * 0.5f) + 1.0f) / 2.0f * color.a);
	DrawRectangle(dx + ctx->fontsize * 0.5f + MeasureRtTextEx(&ctx->font, ctx->search, ctx->fontsize, 0).x, dy + ctx->fontsize * 0.5f, ctx->fontsize / 12.0f, ctx->fontsize, color);
      }
    }

    if (mouse_in_2rect(ctx, main, hitbox)) {
      ctx->cursor = UI_TYPE;
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	ctx->search_focused = true;
	ctx->last_keystroke = GetTime();
      }
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      ctx->search_focused = false;
    }
    
    float x = ctx->fontsize * 0.5f, y = ctx->fontsize * 1.5f;

#define DRAW_ALBUM(a) do {						\
      struct music_album album = (a);					\
      Rectangle hitbox = { dx, dy + y, main.width, ctx->fontsize };	\
      bool hovered = mouse_in_2rect(ctx, main, hitbox);			\
      if (hovered) {							\
	DrawRectangleRec(hitbox, CURRENT_THEME(ctx)->background_p2);	\
	ctx->cursor = UI_CLICK;						\
	if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {	\
	  music_unload(music_ctx);					\
	  da_size(&music_ctx->playlist) = 0;				\
	  for (size_t i = 0; i < da_size(&album.tracklist); i++) music_playlist_push(music_ctx, album.tracklist[i].path); \
	  if (da_size(&album.tracklist)) music_load(music_ctx, album.tracklist[0].path); \
	}								\
      }									\
									\
      DrawRtTextEx(&ctx->font, album.artist, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground); \
      x += MeasureRtTextEx(&ctx->font, album.artist, ctx->fontsize, 0).x;	\
									\
      DrawRtTextEx(&ctx->font, " - ", (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2); \
      x += MeasureRtTextEx(&ctx->font, " - ", ctx->fontsize, 0).x;		\
									\
      DrawRtTextEx(&ctx->font, album.title, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground); \
      x += MeasureRtTextEx(&ctx->font, album.title, ctx->fontsize, 0).x;	\
      									\
      DrawRtTextEx(&ctx->font, TextFormat(" (%d)", album.year), (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2); \
      x += MeasureRtTextEx(&ctx->font, TextFormat(" (%d)", album.year), ctx->fontsize, 0).x; \
    } while (0)

#define DRAW_SONG(s) do {						\
      struct music_song song = (s);					\
      Rectangle hitbox = { dx, dy + y, main.width, ctx->fontsize };	\
      bool hovered = mouse_in_2rect(ctx, main, hitbox);			\
      if (hovered) {							\
	DrawRectangleRec(hitbox, CURRENT_THEME(ctx)->background_p2);	\
	ctx->cursor = UI_CLICK;						\
	if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {	\
	  music_playlist_push(music_ctx, song.path);			\
	  music_load(music_ctx, song.path);				\
	}								\
      }									\
									\
      DrawRtTextEx(&ctx->font, song.artist, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground); \
      x += MeasureRtTextEx(&ctx->font, song.artist, ctx->fontsize, 0).x;	\
									\
      DrawRtTextEx(&ctx->font, " - ", (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2); \
      x += MeasureRtTextEx(&ctx->font, " - ", ctx->fontsize, 0).x;		\
									\
      DrawRtTextEx(&ctx->font, song.title, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground); \
      x += MeasureRtTextEx(&ctx->font, song.title, ctx->fontsize, 0).x;	\
									\
      DrawRtTextEx(&ctx->font, TextFormat(" (%s)", song.album.title), (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2); \
      x += MeasureRtTextEx(&ctx->font, TextFormat(" (%s)", song.album.title), ctx->fontsize, 0).x;	\
    } while (0)
    
    if (da_size(&ctx->search) == 1) {
      for (size_t i = 0; i < da_size(&music_ctx->albums); i++) {
	x = ctx->fontsize * 0.5f;
	y += ctx->fontsize;
	
	DRAW_ALBUM(music_ctx->albums[i]);
	
	for (size_t j = 0; j < da_size(&music_ctx->albums[i].tracklist); j++) {
	  x = ctx->fontsize;
	  y += ctx->fontsize;
	  
	  DRAW_SONG(music_ctx->albums[i].tracklist[j]);
	}
      }
    } else {
      bool song_enter = true;
      bool album_enter = true;
      for (int i = 0; i < ctx->search_ctx->result_count; i++) {
	struct ui_album_song r = ctx->search_ctx->results[i];
	if (r.is_album) {
	  x = ctx->fontsize * 0.5f;
	  y += ctx->fontsize;
	  DRAW_ALBUM(r.v.album);

	  if (album_enter) {
	    album_enter = false;
	    float enter_pos = main.width - MeasureRtTextEx(&ctx->font, "Enter", ctx->fontsize, 0).x - ctx->fontsize;
	    if (enter_pos > x) {
	      DrawRtTextEx(&ctx->font, "Enter", (Vector2) { dx + enter_pos + ctx->fontsize * 0.5f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
	    }

	    if (IsKeyUp(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_ENTER)) {
	      music_unload(music_ctx);
	      da_size(&music_ctx->playlist) = 0;
	      for (size_t i = 0; i < da_size(&r.v.album.tracklist); i++) music_playlist_push(music_ctx, r.v.album.tracklist[i].path);
	      if (da_size(&r.v.album.tracklist)) music_load(music_ctx, r.v.album.tracklist[0].path);
	    }
	  }
	} else {
	  x = ctx->fontsize;
	  y += ctx->fontsize;
	  DRAW_SONG(r.v.song);

	  if (song_enter) {
	    song_enter = false;
	    float enter_pos = main.width - MeasureRtTextEx(&ctx->font, "Shift + Enter", ctx->fontsize, 0).x - ctx->fontsize;
	    if (enter_pos > x) {
	      DrawRtTextEx(&ctx->font, "Shift + Enter", (Vector2) { dx + enter_pos + ctx->fontsize * 0.5f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
	    }
	    
	    if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_ENTER)) {
	      music_playlist_push(music_ctx, r.v.song.path);
	      music_load(music_ctx, r.v.song.path);
	    }
	  }
	}
      }
    }
    
    da_size(&ctx->search)--;

    if (ctx->search_focused) {
      size_t old_size = da_size(&ctx->search);
      int key; while ((key = GetCharPressed())) {
	push_utf8_codepoint(&ctx->search, key);
	ctx->last_keystroke = GetTime();
      }
      if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && da_size(&ctx->search)) {
	while ((da_pop(&ctx->search) & 192) == 128) {}
	ctx->last_keystroke = GetTime();
      }
      if (IsKeyPressed(KEY_ESCAPE)) ctx->search_focused = false;
      
      if (da_size(&ctx->search) != old_size) {
	da_push(&ctx->search, 0);
	
	for (int i = 0; i < UI_TOKENS; i++) ctx->search_ctx->normalized_query[i] = ctx->search_ctx->normalized_query_s[i];
	
	int tokens = tokenize(ctx->search, ctx->search_ctx->normalized_query, UI_TOKENS, UI_STR_LENGTH);
      
	da_size(&ctx->search_ctx->result_ids) = 0;
	for (size_t i = 0; i < da_size(&music_ctx->song_cache); i++) {
	  if (music_ctx->song_cache[i].title == NULL) continue;
	  struct ui_pair value = { i << 1, song_match(music_ctx->song_cache[i], (const char**) ctx->search_ctx->normalized_query, tokens) };
	  if (value.value >= 0) da_push(&ctx->search_ctx->result_ids, value);
	}
	for (size_t i = 0; i < da_size(&music_ctx->albums); i++) {
	  if (music_ctx->albums[i].title == NULL) continue;
	  struct ui_pair value = { (i << 1) | 1, album_match(music_ctx->albums[i], (const char**) ctx->search_ctx->normalized_query, tokens) };
	  if (value.value >= 0) da_push(&ctx->search_ctx->result_ids, value);
	}
	for (size_t i = 0; i < da_size(&ctx->search_ctx->result_ids); i++) {
	  for (size_t j = 0; j < da_size(&ctx->search_ctx->result_ids) - 1; j++) {
	    struct ui_pair a = ctx->search_ctx->result_ids[j];
	    struct ui_pair b = ctx->search_ctx->result_ids[j + 1];
	    if (a.value < b.value) {
	      ctx->search_ctx->result_ids[j] = b;
	      ctx->search_ctx->result_ids[j + 1] = a;
	    }
	  }
	}
	int elements = da_size(&ctx->search_ctx->result_ids);
	if (elements > UI_RESULTS) elements = UI_RESULTS;
	for (int i = 0; i < elements; i++) {
	  struct ui_pair result = ctx->search_ctx->result_ids[i];
	  struct ui_album_song entry = {0};
	  entry.is_album = result.id & 1;
	  if (result.id & 1) entry.v.album = music_ctx->albums[result.id >> 1];
	  else entry.v.song = music_ctx->song_cache[result.id >> 1];
	  ctx->search_ctx->results[i] = entry;
	}
	ctx->search_ctx->result_count = elements;
	
	da_size(&ctx->search)--;
      }
    }
    
    h = y + ctx->fontsize * 1.5f;
  } else if (ctx->page == UI_ALBUMS) {
    float x = ctx->fontsize * 0.5f, y = ctx->fontsize * 0.5f;
    if (da_size(&music_ctx->albums) == 0) DrawRtTextEx(&ctx->font, "*crickets*", (Vector2) {dx + x, dy + y}, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
    for (size_t i = 0; i < da_size(&music_ctx->albums); i++) {
      struct music_album album = music_ctx->albums[i];

      if (dy + y + ctx->fontsize * 9.5f > 0 && dy + y < main.y + main.height) {
	Rectangle hitbox = { dx + x, dy + y, ctx->fontsize * 7.0f, ctx->fontsize * (ctx->settings.dont_load_covers ? 3.0f : 9.5f) };
	bool hovered = mouse_in_2rect(ctx, main, hitbox);
	if (hovered) ctx->cursor = UI_CLICK;
	if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	  ctx->page = UI_ALBUM;
	  ctx->current_album = i;
	}
      
	if (hovered) DrawRectangleRec(hitbox, CURRENT_THEME(ctx)->background_p2);

	if (!ctx->settings.dont_load_covers) DrawTexture(get_cover(ctx, &music_ctx->albums[i].cover), dx + x + ctx->fontsize * 0.5f, dy + y + ctx->fontsize * 0.5f, WHITE);

	Rectangle scissor = { dx + x + ctx->fontsize * 0.5f, dy + y + ctx->fontsize * (ctx->settings.dont_load_covers ? 0.5f : 7.0f), ctx->fontsize * 6.0f, ctx->fontsize * 2.0f };
	scissor = rect_overlap(scissor, main);
	EndScissorMode();
	BeginScissorMode(scissor.x, scissor.y, scissor.width, scissor.height);
	DrawRtTextEx(&ctx->font, album.title, (Vector2) { dx + x + ctx->fontsize * 0.5f, dy + y + ctx->fontsize * (ctx->settings.dont_load_covers ? 0.5f : 7.0f) }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
	DrawRtTextEx(&ctx->font, TextFormat("%s, %d", album.artist, album.year), (Vector2) { dx + x + ctx->fontsize * 0.5f, dy + y + ctx->fontsize * (ctx->settings.dont_load_covers ? 1.5f : 8.0f) }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
	EndScissorMode();
	BeginScissorMode(main.x, main.y, main.width, main.height);
      }

      if (i != da_size(&music_ctx->albums) - 1) {
	if (dx + x + ctx->fontsize * 14.5f > GetScreenWidth()) {
	  x = ctx->fontsize * 0.5f; y += ctx->fontsize * (ctx->settings.dont_load_covers ? 3.0f : 9.5f);
	} else {
	  x += ctx->fontsize * 7.0f;
	}
      }
    }

    h = y + ctx->fontsize * (ctx->settings.dont_load_covers ? 3.5f : 10.0f);
  } else if (ctx->page == UI_SETTINGS) {
    float y = ctx->fontsize * 0.5f;
    draw_separator(ctx, main, dy + y, "General");
    y += ctx->fontsize;
    bool hovered = mouse_in_2rect(ctx, main, ((Rectangle) {dx, dy + y, main.width, ctx->fontsize}));
    if (hovered) ctx->cursor = UI_CLICK;
    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      ctx->settings.dont_load_covers = !ctx->settings.dont_load_covers;
      if (ctx->settings.dont_load_covers) {
	for (size_t i = 0; i < da_size(&ctx->texture_pool); i++) {
	  UnloadTexture(ctx->texture_pool[i]);
	}
	da_size(&ctx->texture_pool) = 0;
	for (size_t i = 0; i < da_size(&music_ctx->song_cache); i++) {
	  music_ctx->song_cache[i].cover.texture = -1;
	}
	for (size_t i = 0; i < da_size(&music_ctx->albums); i++) {
	  music_ctx->albums[i].cover.texture = -1;
	}
      }
    }
    DrawRectangle(dx + ctx->fontsize * 0.5f, dy + y, ctx->fontsize, ctx->fontsize, CURRENT_THEME(ctx)->middleground);
    if (ctx->settings.dont_load_covers) {
      DrawTexture(ctx->icons.check, dx + ctx->fontsize * 0.5f, dy + y, CURRENT_THEME(ctx)->foreground);
    }
    DrawRtTextEx(&ctx->font, "Don't load covers", (Vector2) { dx + ctx->fontsize * 2.0f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    y += ctx->fontsize;
    DrawRtTextEx(&ctx->font, "Greatly reduces VRAM usage", (Vector2) { dx + ctx->fontsize * 2.0f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);

    y += ctx->fontsize * 1.5f;
    draw_separator(ctx, main, dy + y, "Scanning");
    y += ctx->fontsize; DrawRtTextEx(&ctx->font, "Scan Folders: (click to remove)", (Vector2) { dx + ctx->fontsize * 0.5f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    if (da_size(&music_ctx->scan_folders) == 0) {
      y += ctx->fontsize;
      DrawRtTextEx(&ctx->font, "*crickets*", (Vector2) { dx + ctx->fontsize * 1.0f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
    }
    for (size_t i = 0; i < da_size(&music_ctx->scan_folders); i++) {
      Rectangle hitbox = { dx, dy + y + ctx->fontsize, main.width, ctx->fontsize };
      bool hovered = mouse_in_2rect(ctx, main, hitbox);
      if (hovered) {
	DrawRectangleRec(hitbox, CURRENT_THEME(ctx)->background_p2);
	ctx->cursor = UI_CLICK;
      }
      y += ctx->fontsize; DrawRtTextEx(&ctx->font, music_ctx->scan_folders[i].path, (Vector2) { dx + ctx->fontsize, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
      if (!music_ctx->scan_folders[i].scanned) {
	DrawRtTextEx(&ctx->font, "(Unscanned)", (Vector2) { dx + ctx->fontsize * 1.5f + MeasureRtTextEx(&ctx->font, music_ctx->scan_folders[i].path, ctx->fontsize, 0).x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
      }
      if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
	memmove(music_ctx->scan_folders + i, music_ctx->scan_folders + i + 1, (da_size(&music_ctx->scan_folders) - i - 1) * sizeof(music_ctx->scan_folders[0]));
	da_size(&music_ctx->scan_folders) -= 1;
	i--;
      }
    }
    int ephemeral_songs = 0;
    for (size_t i = 0; i < da_size(&music_ctx->song_cache); i++) {
      ephemeral_songs += (int) music_ctx->song_cache[i].ephemeral;
    }
    y += ctx->fontsize; DrawRtTextEx(&ctx->font, TextFormat("Scanned Songs: %zu total, %zu ephemeral", da_size(&music_ctx->song_cache), ephemeral_songs), (Vector2) { dx + ctx->fontsize * 0.5f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    y += ctx->fontsize;
    if (draw_button(ctx, main, (Vector2) { dx + ctx->fontsize * 0.5f, dy + y }, "Clear Song Cache")) {
      music_unload(music_ctx);
      da_size(&music_ctx->song_cache) = 0;
      da_size(&music_ctx->playlist) = 0;
      da_size(&music_ctx->albums) = 0;
      da_size(&music_ctx->play_history) = 0;
      for (size_t i = 0; i < da_size(&music_ctx->scan_folders); i++) music_ctx->scan_folders[i].scanned = false;
    }
    if (draw_button(ctx, main, (Vector2) { dx + ctx->fontsize * 1.5f + MeasureRtTextEx(&ctx->font, "Clear Song Cache", ctx->fontsize, 0).x, dy + y }, "Rescan")) {
      for (size_t i = 0; i < da_size(&music_ctx->scan_folders); i++) {
	music_scan_folder(music_ctx, music_ctx->scan_folders[i].path);
      }
    }

    y += ctx->fontsize * 1.5f;
    draw_separator(ctx, main, dy + y, "User Interface");
    
    y += ctx->fontsize;
    DrawRtTextEx(&ctx->font, "Current theme:", (Vector2) { dx + ctx->fontsize * 0.5f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    float w = MeasureRtTextEx(&ctx->font, "Current theme:", ctx->fontsize, 0).x;
    Rectangle hitbox = { dx + ctx->fontsize + w, dy + y, MeasureRtTextEx(&ctx->font, CURRENT_THEME(ctx)->name, ctx->fontsize, 0).x + ctx->fontsize * 1.5f, ctx->fontsize };
    hovered = mouse_in_2rect(ctx, main, hitbox);
    if (hovered) ctx->cursor = UI_CLICK;
    DrawRectangleRec(hitbox, hovered ? CURRENT_THEME(ctx)->middleground_p1 : CURRENT_THEME(ctx)->middleground);
    DrawRtTextEx(&ctx->font, CURRENT_THEME(ctx)->name, (Vector2) { hitbox.x + ctx->fontsize * 0.25f, hitbox.y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    DrawTexture(ctx->dropdown.open ? ctx->icons.arrow_up : ctx->icons.arrow_down, hitbox.x + hitbox.width - ctx->fontsize, hitbox.y, CURRENT_THEME(ctx)->foreground);
    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      ctx->dropdown.open = !ctx->dropdown.open;
      ctx->dropdown.rect = (Rectangle) { hitbox.x, hitbox.y + hitbox.height, hitbox.width, ctx->fontsize * da_size(&ctx->themes) };

      if (ctx->dropdown.rect.y + ctx->dropdown.rect.height >= GetScreenHeight()) ctx->dropdown.rect.y -= ctx->dropdown.rect.height + ctx->fontsize;
      
      for (size_t i = 0; i < da_size(&ctx->themes); i++) {
	float width = get_text_width(ctx, ctx->themes[i].name) + ctx->fontsize;
	if (width > ctx->dropdown.rect.width) ctx->dropdown.rect.width = width;
      }
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !mouse_in_dropdown(ctx)) ctx->dropdown.open = false;
    y += ctx->fontsize;
    
    DrawRtTextEx(&ctx->font, TextFormat("by %s", CURRENT_THEME(ctx)->author), (Vector2) { dx + ctx->fontsize * 0.5f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
    y += ctx->fontsize * 1.25f;

    DrawRectangle(dx + ctx->fontsize * 0.5f, dy + y, ctx->fontsize, ctx->fontsize, CURRENT_THEME(ctx)->background);
    DrawRectangle(dx + ctx->fontsize * 1.5f, dy + y, ctx->fontsize, ctx->fontsize, CURRENT_THEME(ctx)->background_p1);
    DrawRectangle(dx + ctx->fontsize * 2.5f, dy + y, ctx->fontsize, ctx->fontsize, CURRENT_THEME(ctx)->background_p2);
    DrawRectangle(dx + ctx->fontsize * 3.5f, dy + y, ctx->fontsize, ctx->fontsize, CURRENT_THEME(ctx)->middleground);
    DrawRectangle(dx + ctx->fontsize * 4.5f, dy + y, ctx->fontsize, ctx->fontsize, CURRENT_THEME(ctx)->middleground_p1);
    DrawRectangle(dx + ctx->fontsize * 5.5f, dy + y, ctx->fontsize, ctx->fontsize, CURRENT_THEME(ctx)->foreground_m2);
    DrawRectangle(dx + ctx->fontsize * 6.5f, dy + y, ctx->fontsize, ctx->fontsize, CURRENT_THEME(ctx)->foreground_m1);
    DrawRectangle(dx + ctx->fontsize * 7.5f, dy + y, ctx->fontsize, ctx->fontsize, CURRENT_THEME(ctx)->foreground);
    DrawRectangle(dx + ctx->fontsize * 8.5f, dy + y, ctx->fontsize, ctx->fontsize, CURRENT_THEME(ctx)->link);

    if (ctx->dropdown.open) {
      EndScissorMode();
      BeginScissorMode(ctx->dropdown.rect.x, ctx->dropdown.rect.y, ctx->dropdown.rect.width, ctx->dropdown.rect.height);

      int new_theme = draw_dropdown_themes(ctx);
      if (new_theme >= 0) set_theme(ctx, new_theme);
      
      EndScissorMode();
      BeginScissorMode(main.x, main.y, main.width, main.height);
    }

    y += ctx->fontsize * 1.5f;
    draw_separator(ctx, main, dy + y, "Debug");
    y += ctx->fontsize; DrawRtTextEx(&ctx->font, TextFormat("FPS: %d Hz (FT: %.4f s, 1/FT: %.2f Hz)", GetFPS(), GetFrameTime(), 1/GetFrameTime()), (Vector2) { dx + ctx->fontsize * 0.5f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    y += ctx->fontsize; DrawRtTextEx(&ctx->font, TextFormat("Font size: %.1fpx (DPI: %.1fx%.1fpx)", ctx->fontsize, GetWindowScaleDPI().x, GetWindowScaleDPI().y), (Vector2) { dx + ctx->fontsize * 0.5f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    y += ctx->fontsize; DrawRtTextEx(&ctx->font, TextFormat("Window size: %dx%dpx", GetScreenWidth(), GetScreenHeight()), (Vector2) { dx + ctx->fontsize * 0.5f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    const char *cursor_text = TextFormat("Cursor XY: %dx%dpx", GetMouseX(), GetMouseY());
    y += ctx->fontsize; DrawRtTextEx(&ctx->font, cursor_text, (Vector2) { dx + ctx->fontsize * 0.5f, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    if (!IsCursorOnScreen()) {
      DrawRtTextEx(&ctx->font, " (Cursor not in the client area)", (Vector2) { dx + ctx->fontsize * 0.5f + MeasureRtTextEx(&ctx->font, cursor_text, ctx->fontsize, 0).x, dy + y }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
    }

    h = y + ctx->fontsize * 1.5f;
  } else if (ctx->page == UI_ABOUT) {
    char *tmpsb = da_new(ctx->arena, char);
    char *tmpurlsb = da_new(ctx->arena, char);
    bool dim = false, url = false;
    float y = 0.5f * ctx->fontsize, x = 0.5f * ctx->fontsize;

#define FLUSH DrawRtTextEx(&ctx->font, tmpsb, (Vector2) { dx + x, dy + y }, ctx->fontsize, 0, url ? CURRENT_THEME(ctx)->link : dim ? CURRENT_THEME(ctx)->foreground_m2 : CURRENT_THEME(ctx)->foreground)
    
    const char *start = (const char*) _ABOUT;
    const char *end = (const char*) (_ABOUT + _ABOUT_LENGTH);
    while (start < end) {
      if (*start == '*') {
	da_push(&tmpsb, 0);
	FLUSH;
	x += MeasureRtTextEx(&ctx->font, tmpsb, ctx->fontsize, 0).x;
	da_size(&tmpsb) = 0;
	dim = !dim;
	start++;
      } else if (*start == '\n') {
	da_push(&tmpsb, 0);
	FLUSH;
	da_size(&tmpsb) = 0;
	x = ctx->fontsize * 0.5f; y += ctx->fontsize;
	start++;
      } else if (*start == '[') {
	da_push(&tmpsb, 0);
	FLUSH;
	x += MeasureRtTextEx(&ctx->font, tmpsb, ctx->fontsize, 0).x;
	da_size(&tmpsb) = 0;
	start++; while (start < end && *start != ']') da_push(&tmpsb, *(start++)); start++;
	if (start < end && *start == '(') {
	  da_size(&tmpurlsb) = 0;
	  start++; while (start < end && *start != ')') da_push(&tmpurlsb, *(start++)); start++;
	  da_push(&tmpurlsb, 0);

	  bool hovered = mouse_in_2rect(ctx, main, ((Rectangle) { dx + x, dy + y, MeasureRtTextEx(&ctx->font, tmpsb, ctx->fontsize, 0).x, ctx->fontsize }));
	  if (hovered) {
	    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) OpenURL(tmpurlsb);
	    ctx->cursor = UI_CLICK;
	  }
	
	  da_push(&tmpsb, 0);
	  url = true;
	  FLUSH;
	  url = false;
	} else {
	  da_push(&tmpsb, 0);
	  da_push(&tmpsb, 0);
	  da_push(&tmpsb, 0);
	  memmove(tmpsb + 1, tmpsb, da_size(&tmpsb) - 3);
	  tmpsb[0] = '[';
	  tmpsb[da_size(&tmpsb) - 2] = ']';
	  FLUSH;
	}
	
	x += MeasureRtTextEx(&ctx->font, tmpsb, ctx->fontsize, 0).x;
	da_size(&tmpsb) = 0;
      } else {
	da_push(&tmpsb, *(start++));
      }
    }
    da_push(&tmpsb, 0);
    FLUSH;
    y += ctx->fontsize * 0.5f; h = y;
#undef FLUSH
  }
  
  draw_scrollbar(ctx, main, ctx->scroll[ctx->page], h);
  handle_scroll(ctx, main, &ctx->scroll[ctx->page], h);
  
  EndScissorMode();
}

void draw_play_control(struct ui_context *ctx, struct music_context *music_ctx) {
  Rectangle pc = { 0, GetScreenHeight() - ctx->fontsize * 3.5f, GetScreenWidth(), ctx->fontsize * 3.5f };
  BeginScissorMode(pc.x, pc.y, pc.width, pc.height);
  ClearBackground(CURRENT_THEME(ctx)->background_p2);

  Vector2 mouse = GetMousePosition();
  
  float x = ctx->fontsize * 0.5f;
  float start_x = x;
  if (music_ctx->current_song_idx < 0) {
    DrawRtTextEx(&ctx->font, "Drag and drop a song to load it, or drag and drop a folder to scan it.", (Vector2) { pc.x + x, pc.y + 0.5f * ctx->fontsize }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
    x = pc.width - ctx->fontsize * 0.5f;
  } else {
    struct music_song current_song = music_ctx->song_cache[music_ctx->current_song_idx];
    bool no_tags = current_song.title == NULL;
    if (current_song.title		== NULL) current_song.title		= "<null>";
    if (current_song.artist		== NULL) current_song.artist		= "<null>";
    if (current_song.album.title	== NULL) current_song.album.title	= "<null>";
    if (current_song.album.artist	== NULL) current_song.album.artist	= "<null>";

    if (no_tags) {
      DrawRtTextEx(&ctx->font, basename((char*) current_song.path), (Vector2) { pc.x + x, pc.y + 0.5f * ctx->fontsize }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    } else {
      DrawRtTextEx(&ctx->font, current_song.artist, (Vector2) { pc.x + x, pc.y + 0.5f * ctx->fontsize }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
      x += MeasureRtTextEx(&ctx->font, current_song.artist, ctx->fontsize, 0).x;
      DrawRtTextEx(&ctx->font, " - ", (Vector2) { pc.x + x, pc.y + 0.5f * ctx->fontsize }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
      x += MeasureRtTextEx(&ctx->font, " - ", ctx->fontsize, 0).x;
      DrawRtTextEx(&ctx->font, current_song.title, (Vector2) { pc.x + x, pc.y + 0.5f * ctx->fontsize }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
      x += MeasureRtTextEx(&ctx->font, current_song.title, ctx->fontsize, 0).x;
      float old_x = x;
      x = GetScreenWidth() - ctx->fontsize * 0.5f;
      x -= MeasureRtTextEx(&ctx->font, current_song.album.title, ctx->fontsize, 0).x;
      if (old_x < x - ctx->fontsize * 0.5f) {
	DrawRtTextEx(&ctx->font, current_song.album.title, (Vector2) { pc.x + x, pc.y + 0.5f * ctx->fontsize }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
      }
    }
    x = ctx->fontsize * 0.5f;
    const char *seek = seconds_to_text(music_get_seek(music_ctx)), *length = seconds_to_text(music_get_length(music_ctx));
    DrawRtTextEx(&ctx->font, seek, (Vector2) { pc.x + x, pc.y + 2.0f * ctx->fontsize }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    x += MeasureRtTextEx(&ctx->font, seek, ctx->fontsize, 0).x + ctx->fontsize * 0.25f;
    DrawRtTextEx(&ctx->font, "/", (Vector2) { pc.x + x, pc.y + 2.0f * ctx->fontsize }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground_m2);
    x += MeasureRtTextEx(&ctx->font, "/", ctx->fontsize, 0).x + ctx->fontsize * 0.25f;
    DrawRtTextEx(&ctx->font, length, (Vector2) { pc.x + x, pc.y + 2.0f * ctx->fontsize }, ctx->fontsize, 0, CURRENT_THEME(ctx)->foreground);
    x += MeasureRtTextEx(&ctx->font, length, ctx->fontsize, 0).x + ctx->fontsize * 0.5f;
    start_x = x;
    x = pc.width - ctx->fontsize * 0.5f;
    bool hovered = false;
    for (int b = 0; b < 4; b++) {
      bool disabled = false;
      if (b == 3) disabled = !music_playlist_can_prev(music_ctx);
      if (b == 1) disabled = !music_playlist_can_next(music_ctx);
      x -= ctx->fontsize;
      hovered = mouse_in_rect(ctx, ((Rectangle) { pc.x + x, pc.y + 2.0f * ctx->fontsize, ctx->fontsize, ctx->fontsize }));
      if (hovered && !disabled) {
	DrawRectangle(pc.x + x, pc.y + 2.0f * ctx->fontsize, ctx->fontsize, ctx->fontsize, IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? CURRENT_THEME(ctx)->foreground : CURRENT_THEME(ctx)->foreground_m2);
	ctx->cursor = UI_CLICK;
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !disabled) {
	  if (b == 3) music_playlist_prev(music_ctx);
	  if (b == 2) music_set_pause(music_ctx, !music_get_pause(music_ctx));
	  if (b == 1) music_playlist_next(music_ctx);
	  if (b == 0) music_set_looping(music_ctx, (music_ctx->looping + 1) % MUSIC_LOOPING_COUNT);
	}
      }
      Texture texture;
      if (b == 3) texture = ctx->icons.previous;
      if (b == 2) texture = music_get_pause(music_ctx) ? ctx->icons.play : ctx->icons.pause;
      if (b == 1) texture = ctx->icons.next;
      if (b == 0) {
	switch (music_ctx->looping) {
	case MUSIC_LINEAR: texture = ctx->icons.repeat.linear; break;
	case MUSIC_REPEAT_PLAYLIST: texture = ctx->icons.repeat.playlist; break;
	case MUSIC_REPEAT_ONE: texture = ctx->icons.repeat.one; break;
	case MUSIC_SHUFFLE: texture = ctx->icons.repeat.shuffle; break;
	default: texture = ctx->icons.pause;
	}
      }
      DrawTexture(texture, pc.x + x, pc.y + 2.0f * ctx->fontsize, disabled ? CURRENT_THEME(ctx)->foreground_m2 : hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? CURRENT_THEME(ctx)->background_p2 : CURRENT_THEME(ctx)->foreground);
    }
    x -= ctx->fontsize * 0.5f;
  }
  Rectangle hitbox = { pc.x + start_x, pc.y + 2.25f * ctx->fontsize, x - start_x, 0.5f * ctx->fontsize };
  float height = music_ctx->current_song_idx >= 0 && (mouse_in_rect(ctx, hitbox) || ctx->pc.seeking) ? ctx->fontsize * 0.25f : ctx->fontsize * 0.125f;
  if (music_ctx->current_song_idx >= 0 && CheckCollisionPointRec(mouse, hitbox)) {
    ctx->cursor = UI_CLICK;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      ctx->pc.pause_after_seeking = music_get_pause(music_ctx);
      music_set_pause(music_ctx, true);
      ctx->pc.seeking = true;
    }
  }
  if (ctx->pc.seeking) {
    float seek = (mouse.x - hitbox.x) / hitbox.width;
    if (seek < 0.0f) seek = 0.0f;
    if (seek > 1.0f) seek = 1.0f;
    music_set_seek(music_ctx, seek * music_get_length(music_ctx));
    if (IsMouseButtonUp(MOUSE_BUTTON_LEFT)) {
      music_set_pause(music_ctx, ctx->pc.pause_after_seeking);
      ctx->pc.seeking = false;
    }
  }
  DrawRectangle(pc.x + start_x, pc.y + 2.5f * ctx->fontsize - height * 0.5f, (x - start_x), height, CURRENT_THEME(ctx)->foreground_m2);
  if (music_ctx->current_song_idx >= 0 && music_get_length(music_ctx) > 0.0f) {
    DrawRectangle(pc.x + start_x, pc.y + 2.5f * ctx->fontsize - height * 0.5f, (x - start_x) * (music_get_seek(music_ctx) / music_get_length(music_ctx)), height, CURRENT_THEME(ctx)->foreground);
  }
  EndScissorMode();
}

void draw_ui(struct ui_context *ctx, struct music_context *music_ctx) {
  BeginDrawing();
  
  music_load_one_from_queue(music_ctx);
  ctx->cover_loaded_this_frame = false;
  load_texture(ctx);

  if (IsFileDropped()) {
    FilePathList dropped = LoadDroppedFiles();

    for (size_t i = 0; i < dropped.count; i++) {
      const char *path = dropped.paths[i];
      if (strlen(path) >= 7 && memcmp(path, "file://", 7) == 0) path += 7;
      char *path_copy = a_malloc(ctx->arena, strlen(path) + 1);
      memcpy(path_copy, path, strlen(path) + 1);
      if (DirectoryExists(path_copy)) {
	printf("Scanning folder `%s`!\n", path_copy);
	music_scan_folder(music_ctx, path_copy);
      } else {
	printf("Adding ephemeral `%s`!\n", path_copy);
	music_playlist_push(music_ctx, path_copy);
      }
    }
    
    UnloadDroppedFiles(dropped);
  }

  if (ctx->page != UI_BROWSE) ctx->search_focused = false;
  
  if (key_pressed(ctx, KEY_SPACE)) music_set_pause(music_ctx, !music_get_pause(music_ctx));
  if (key_pressed(ctx, KEY_LEFT) && music_playlist_can_prev(music_ctx)) music_playlist_prev(music_ctx);
  if (key_pressed(ctx, KEY_RIGHT) && music_playlist_can_next(music_ctx)) music_playlist_next(music_ctx);
  if (key_pressed(ctx, KEY_R)) music_set_looping(music_ctx, (music_ctx->looping + 1) % MUSIC_LOOPING_COUNT);

  if (key_pressed(ctx, KEY_ONE)) ctx->page = UI_PLAYLIST;
  if (key_pressed(ctx, KEY_TWO)) ctx->page = UI_ALBUMS;
  if (key_pressed(ctx, KEY_THREE)) { ctx->search_focused = true; ctx->page = UI_BROWSE; }
  if (key_pressed(ctx, KEY_FOUR)) ctx->page = UI_SETTINGS;
  if (key_pressed(ctx, KEY_FIVE)) ctx->page = UI_ABOUT;
  
  ctx->cursor = UI_DEFAULT;

  if (ctx->rmb_menu.open && !mouse_in_rmb_menu(ctx) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ctx->rmb_menu.open = false;

  draw_sidebar(ctx, music_ctx);
  draw_play_control(ctx, music_ctx);
  draw_main(ctx, music_ctx);

  switch (ctx->cursor) {
  case UI_DEFAULT: SetMouseCursor(MOUSE_CURSOR_ARROW); break;
  case UI_CLICK: SetMouseCursor(MOUSE_CURSOR_POINTING_HAND); break;
  case UI_SCROLL: SetMouseCursor(MOUSE_CURSOR_RESIZE_NS); break;
  case UI_TYPE: SetMouseCursor(MOUSE_CURSOR_IBEAM); break;
  default: SetMouseCursor(MOUSE_CURSOR_NOT_ALLOWED); break;
  }

  EndDrawing();
}

bool ui_done(struct ui_context *ctx) {
  (void) ctx;
  return WindowShouldClose();
}
