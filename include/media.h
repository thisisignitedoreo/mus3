#pragma once

#include "music.h"

#include <stdbool.h>

bool media_setup(struct music_context *ctx);

enum media_event {
  EVENT_NONE = 0,
  EVENT_ERROR,
};

enum media_event media_tick(void);

enum media_changes {
  CHANGE_SEEK		= 1 << 0,
  CHANGE_PLAYING	= 1 << 1,
  CHANGE_LOOP_MODE	= 1 << 2,
  CHANGE_VOLUME		= 1 << 3,
  CHANGE_CURRENT_TRACK  = 1 << 4,
  CHANGE_ALL  = (1 << 5) - 1,
};

void media_update(enum media_changes changes);

void media_destroy(void);
