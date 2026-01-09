#include <allocator.h>
#include <arena.h>

#include "ui.h"
#include "music.h"
#include "savestate.h"
#include "media.h"

#include <stdlib.h>
#include <time.h>

const char *__asan_default_options(void) { return "detect_leaks=0"; }

int main(void) {
  srand(time(NULL));
  
  struct allocator arena = arena_new(stdc_allocator(), 8*1024);
  
  struct music_context music;
  if (setup_music(&music, arena)) return 1;

  const char *savestate_file = get_savestate_file();
  const char *savestate_folder = get_savestate_folder();

  bool media_inited = media_setup(&music);
  
  struct ui_context ui;
  if (setup_ui(arena, &ui)) goto unload;
  
  load_from_savestate(&music, &ui, savestate_file);
  
  while (!ui_done(&ui)) {
    music_tick(&music, media_inited);
    draw_ui(&ui, &music);
  }

 unload:
  destroy_ui(&ui);

  if (!DirectoryExists(savestate_folder)) MakeDirectory(savestate_folder);
  save_to_savestate(&music, &ui, savestate_file);

  if (media_inited) media_destroy();
  destroy_music(&music);

  arena_free(arena);
  
  return 0;
}
