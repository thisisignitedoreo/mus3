#pragma once

#include "music.h"
#include "ui.h"

const char *get_savestate_folder(void);
const char *get_savestate_file(void);

bool save_to_savestate(struct music_context *ctx, struct ui_context *ui, const char *path);
bool load_from_savestate(struct music_context *ctx, struct ui_context *ui, const char *path);
