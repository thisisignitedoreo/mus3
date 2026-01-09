#include "media.h"

bool media_setup(struct music_context *ctx) { return true; }

enum media_event media_tick(void) { return EVENT_NONE; }

void media_update(enum media_changes changes) { }

void media_destroy(void) {}
