#pragma once

#include "music.h"

#include <allocator.h>

struct music_song tags_load(struct allocator arena, const char *path);
