#include "tags.h"

#include "stuff.h"

#include <dynamic-array.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <libgen.h>

#define ID3_UNSYNC 0x80
#define ID3_EXTHDR 0x40
#define ID3_EXPRMN 0x20

#define ID3_STR_ISO88591 0
#define ID3_STR_UTF16 1
#define ID3_STR_UTF16BE 2
#define ID3_STR_UTF8 3

#define ID3_PIC_COVER_FRONT 0x03

const char *parse_str(struct allocator arena, uint8_t encoding, long end, FILE *f) {
  char *string = da_new(arena, char);
  if (encoding == ID3_STR_ISO88591) {
    uint8_t last_char = 1;
    while (ftell(f) < end && last_char) {
      if (!fread(&last_char, 1, 1, f)) return NULL;
      push_utf8_codepoint(&string, last_char);
    }
  } else if (encoding == ID3_STR_UTF16) {
    uint16_t last_codepoint;
    if (!fread(&last_codepoint, 2, 1, f)) return NULL;
    bool swapendian = last_codepoint == 0xFFFE;
    while (ftell(f) < end && last_codepoint) {
      if (!fread(&last_codepoint, 2, 1, f)) return NULL;
      if (swapendian) last_codepoint = shuffle16(last_codepoint);
      if (0xD800 <= last_codepoint && last_codepoint <= 0xDFFF) {
	uint16_t low_surragate;
	if (!fread(&low_surragate, 2, 1, f)) return NULL;
	if (swapendian) low_surragate = shuffle16(low_surragate);
	last_codepoint = 0x10000 + (((last_codepoint - 0xD800) << 0xA) | (low_surragate - 0xDC00));
      }
      push_utf8_codepoint(&string, last_codepoint);
    }
  } else if (encoding == ID3_STR_UTF16BE) {
    uint16_t last_codepoint = 1;
    bool swapendian = true;
    while (ftell(f) < end && last_codepoint) {
      if (!fread(&last_codepoint, 2, 1, f)) return NULL;
      if (swapendian) last_codepoint = shuffle16(last_codepoint);
      if (0xD800 <= last_codepoint && last_codepoint <= 0xDFFF) {
	uint16_t low_surragate;
	if (!fread(&low_surragate, 2, 1, f)) return NULL;
	if (swapendian) low_surragate = shuffle16(low_surragate);
	last_codepoint = 0x10000 + (((last_codepoint - 0xD800) << 0xA) | (low_surragate - 0xDC00));
      }
      push_utf8_codepoint(&string, last_codepoint);
    }
  } else if (encoding == ID3_STR_UTF8) {
    uint8_t last_char = 1;
    while (ftell(f) < end && last_char) {
      if (!fread(&last_char, 1, 1, f)) return NULL;
      da_push(&string, last_char);
    }
  }
  if (!da_size(&string) || string[da_size(&string) - 1] != 0) da_push(&string, 0);
  return string;
}

const char *parse_str_frame(struct allocator arena, long end, FILE *f) {
  uint8_t enc; if (!fread(&enc, 1, 1, f)) return NULL;
  return parse_str(arena, enc, end, f);
}

bool parse_pic_frame(struct allocator arena, struct music_cover *cover, long end, FILE *f) {
  uint8_t enc; if (!fread(&enc, 1, 1, f)) return true;
  const char *mimetype = parse_str(arena, enc, end, f);
  (void) parse_str(arena, enc, end, f); // Skip description
  uint8_t type; if (!fread(&type, 1, 1, f)) return true;
  // TODO: support other stuff
  // TODO: fucking id3v2 stupid shit mp3 stuff duck fuck you all aaaa
  // if (type != ID3_PIC_COVER_FRONT) {
  //   printf("DEBUG: id3 pic type: %d\n", type);
  //   return true;
  // }
  size_t picture_size = end - ftell(f);
  const char *ext =
    strcmp(mimetype, "image/png") == 0 || strcmp(mimetype, "png") == 0 ? ".png" :
    strcmp(mimetype, "image/jpg") == 0 || strcmp(mimetype, "jpg") == 0 ||
    strcmp(mimetype, "image/jpeg") == 0 || strcmp(mimetype, "jpeg") == 0 ? ".jpg" :
    NULL;
  if (ext == NULL) {
    printf("WARN: unsupported APIC extension mimetype `%s`\n", mimetype);
    return true;
  }

  cover->exists = true;
  cover->offset = ftell(f);
  cover->size = picture_size;
  cover->ext = ext;
  return false;
}

struct music_song tags_load_id3v2(struct allocator arena, const char *path) {
  struct music_song song = { .path = path };
  FILE *f = file_open(path, "rb");
  if (f == NULL) return (struct music_song) { path };

  uint8_t buf[8];

  if (!fread(buf, 3, 1, f) || memcmp(buf, "ID3", 3) != 0) { fclose(f); return (struct music_song) { path }; }
  if (!fread(buf, 2, 1, f) || buf[0] != 4) {
    printf("%s: WARN: ID3v2.%d.%d is not supported\n", basename((char*) path), buf[0], buf[1]);
  }

  uint8_t flags;
  if (!fread(&flags, 1, 1, f)) { fclose(f); return (struct music_song) { path }; }

  // TODO: support unsynchronization
  if (flags & ID3_UNSYNC) {
    printf("%s: TODO: support unsynchronization\n", basename((char*) path));
    fclose(f);
    return (struct music_song) { path };
  }

  uint32_t size; if (read28be(f, &size)) { fclose(f); return (struct music_song) { path }; }

  // Skip extended header
  if (flags & ID3_EXTHDR) {
    uint32_t extsize; if (read32be(f, &extsize)) { fclose(f); return (struct music_song) { path }; }
    fseek(f, extsize, SEEK_CUR);
  }

  while ((unsigned long) ftell(f) < 10 + size) {
    uint8_t frame_hdr[10];
    if (!fread(frame_hdr, 10, 1, f)) { fclose(f); return (struct music_song) { path }; }
    uint32_t frame_size = parse32be(&frame_hdr[4]);
    long end = ftell(f) + frame_size;

    if (memcmp(frame_hdr, "\0\0\0\0", 4) == 0) break;

    if (frame_hdr[9] & 0x1F) printf("%s: WARN: top bits of the frame flags second bits are set, frame likely corrupted\n", basename((char*) path));
    else if (memcmp(frame_hdr, "TIT2", 4) == 0) song.title = parse_str_frame(arena, end, f);
    else if (memcmp(frame_hdr, "TPE1", 4) == 0) song.artist = parse_str_frame(arena, end, f);
    else if (memcmp(frame_hdr, "TALB", 4) == 0) song.album.title = parse_str_frame(arena, end, f);
    else if (memcmp(frame_hdr, "TPE2", 4) == 0) song.album.artist = parse_str_frame(arena, end, f);
    else if (memcmp(frame_hdr, "TYER", 4) == 0) song.year = atoi(parse_str_frame(arena, end, f));
    else if (memcmp(frame_hdr, "TDRL", 4) == 0) song.year = atoi(parse_str_frame(arena, end, f));
    else if (memcmp(frame_hdr, "TLEN", 4) == 0) song.length = atoi(parse_str_frame(arena, end, f));
    else if (memcmp(frame_hdr, "TRCK", 4) == 0) {
      const char *track = parse_str_frame(arena, end, f);
      while (*track == '0') track++;
      song.track.this = atoi(track);
      while (isdigit(*track)) track++;
      if (*track == '/') {
	track++;
	while (*track == '0') track++;
	song.track.all = atoi(track);
      }
    } else if (memcmp(frame_hdr, "TPOS", 4) == 0) {
      const char *track = parse_str_frame(arena, end, f);
      while (*track == '0') track++;
      song.disc.this = atoi(track);
      while (isdigit(*track)) track++;
      if (*track == '/') {
	track++;
	while (*track == '0') track++;
	song.disc.all = atoi(track);
      }
    } else if (memcmp(frame_hdr, "APIC", 4) == 0) {
      if (parse_pic_frame(arena, &song.cover, end, f)) {
	printf("%s: WARN: error parsing the APIC frame\n", basename((char*) path));
      }
      song.cover.path = path;
    }

    fseek(f, end, SEEK_SET);
  }

  fclose(f);
  
  return song;
}

bool parse_vorbis_comment(struct allocator arena, struct music_song *song, long end, FILE *f) {
  uint32_t vendor_size; if (read32le(f, &vendor_size)) return true;
  fseek(f, vendor_size, SEEK_CUR); // Skip the vendor string
  
  uint32_t comments_count; if (read32le(f, &comments_count)) return true;

  char *title = NULL, *album = NULL,
    *albumartist = NULL, *artist = NULL, *date = NULL,
    *tracknumber = NULL, *discnumber = NULL,
    *tracktotal = NULL, *disctotal = NULL;
  
  char *key = da_new(arena, char);
  
  while (ftell(f) < end) {
    uint32_t comment_length; if (read32le(f, &comment_length)) return true;
    long start = ftell(f);
    
    da_size(&key) = 0;
    char k = 0;
    if (!fread(&k, 1, 1, f)) return true;
    while (k != '=') {
      da_push(&key, tolower(k));
      if (!fread(&k, 1, 1, f)) return true;
    }
    da_push(&key, 0);

    char **field = NULL;

    if (strcmp(key, "title") == 0) field = &title;
    else if (strcmp(key, "album") == 0) field = &album;
    else if (strcmp(key, "track") == 0) field = &tracknumber;
    else if (strcmp(key, "tracknumber") == 0) field = &tracknumber;
    else if (strcmp(key, "tracktotal") == 0) field = &tracktotal;
    else if (strcmp(key, "artist") == 0) field = &artist;
    else if (strcmp(key, "albumartist") == 0) field = &albumartist;
    else if (strcmp(key, "disc") == 0) field = &discnumber;
    else if (strcmp(key, "discnumber") == 0) field = &discnumber;
    else if (strcmp(key, "date") == 0) field = &date;
    else {
      fseek(f, start + comment_length, SEEK_SET);
      continue;
    }

    if (*field == NULL) *field = da_new(arena, char);
    else { da_push(field, ','); da_push(field, ' '); }

    size_t field_size = comment_length - ftell(f) + start;
    da_reserve(field, field_size);
    if (!fread(*field + da_size(field) - field_size, field_size, 1, f)) return true;
  }

  if (title) { da_push(&title, 0); song->title = title; }
  if (album) { da_push(&album, 0); song->album.title = album; }
  if (artist) { da_push(&artist, 0); song->artist = artist; }
  if (albumartist) { da_push(&albumartist, 0); song->album.artist = albumartist; }
  if (tracknumber) {
    da_push(&tracknumber, 0);
    while (*tracknumber == '0') tracknumber++;
    song->track.this = atoi(tracknumber);
    while (isdigit(*tracknumber)) tracknumber++;
    if (*tracknumber == '/') {
      tracknumber++;
      while (*tracknumber == '0') tracknumber++;
      song->track.all = atoi(tracknumber);
    }
  }
  if (discnumber) {
    da_push(&discnumber, 0);
    while (*discnumber == '0') discnumber++;
    song->disc.this = atoi(discnumber);
    while (isdigit(*discnumber)) discnumber++;
    if (*discnumber == '/') {
      discnumber++;
      while (*discnumber == '0') discnumber++;
      song->disc.all = atoi(discnumber);
    }
  }
  if (tracktotal) {
    da_push(&tracktotal, 0);
    while (*tracktotal == '0') tracktotal++;
    song->track.all = atoi(tracktotal);
    while (isdigit(*tracktotal)) tracktotal++;
  }
  if (disctotal) {
    da_push(&disctotal, 0);
    while (*disctotal == '0') disctotal++;
    song->disc.all = atoi(disctotal);
    while (isdigit(*disctotal)) disctotal++;
  }
  if (date) {
    da_push(&date, 0);
    song->year = atoi(date);
  }
  
  return false;
}

#define FLAC_STREAMINFO 0
#define FLAC_VORBISCOMMENT 4
#define FLAC_PICTURE 6

struct music_song tags_load_flac(struct allocator arena, const char *path) {
  struct music_song song = {0};
  song.path = path;
  
  FILE *f = file_open(path, "rb");
  if (f == NULL) return (struct music_song) { path };

  fseek(f, 4, SEEK_SET); // == b"fLaC"

  bool last = false;
  while (!last) {
    uint8_t type; if (!fread(&type, 1, 1, f)) return (struct music_song) { path };
    last = (type & 128) == 128;
    type &= 127;
    uint32_t length; if (read24be(f, &length)) return (struct music_song) { path };
    long start = ftell(f);

    if (type == FLAC_STREAMINFO) {
      uint8_t hdr[18];
      if (!fread(hdr, 18, 1, f)) return (struct music_song) { path };
      
      uint32_t sample_rate =
	((uint32_t)hdr[10] << 12) |
	((uint32_t)hdr[11] << 4)  |
	((uint32_t)hdr[12] >> 4);

      uint64_t total_samples =
	((uint64_t)(hdr[13] & 0x0F) << 32) |
	((uint64_t)hdr[14] << 24) |
	((uint64_t)hdr[15] << 16) |
	((uint64_t)hdr[16] << 8)  |
	((uint64_t)hdr[17]);

      song.length = total_samples * 1000 / sample_rate;
    } else if (type == FLAC_VORBISCOMMENT) {
      if (parse_vorbis_comment(arena, &song, start + length, f)) return (struct music_song) { path };
    } else if (type == FLAC_PICTURE) {
      uint32_t type; if (read32be(f, &type)) return (struct music_song) { path };
      if (type == ID3_PIC_COVER_FRONT) {
	uint32_t mimetype_length; if (read32be(f, &mimetype_length)) return (struct music_song) { path };
	char *mimetype = a_malloc(arena, mimetype_length + 1);
	if (!fread(mimetype, mimetype_length, 1, f)) return (struct music_song) { path };
	mimetype[mimetype_length] = 0;
	const char *ext =
	  strcmp(mimetype, "image/png") == 0 || strcmp(mimetype, "png") == 0 ? ".png" :
	  strcmp(mimetype, "image/jpg") == 0 || strcmp(mimetype, "jpg") == 0 ||
	  strcmp(mimetype, "image/jpeg") == 0 || strcmp(mimetype, "jpeg") == 0 ? ".jpg" :
	  NULL;
	uint32_t desc_length; if (read32be(f, &desc_length)) return (struct music_song) { path };
	fseek(f, 16 + desc_length, SEEK_CUR);
	uint32_t picture_length; if (read32be(f, &picture_length)) return (struct music_song) { path };
	
	song.cover.exists = true;
	song.cover.path = path;
	song.cover.offset = ftell(f);
	song.cover.size = picture_length;
	song.cover.ext = ext;
      }
    }

    fseek(f, start + length, SEEK_SET);
  }
  
  fclose(f);
  return song;
}

struct music_song tags_load(struct allocator arena, const char *path) {
  FILE *f = file_open(path, "rb");
  if (f == NULL) return (struct music_song) { path };
  char magic[4];
  if (!fread(&magic, 4, 1, f)) {
    fclose(f);
    return (struct music_song) { path };
  }
  fclose(f);

  struct music_song tags = {0};
  
  if (memcmp(magic, "ID3", 3) == 0) {
    tags = tags_load_id3v2(arena, path);
  } else if (memcmp(magic, "fLaC", 4) == 0) {
    tags = tags_load_flac(arena, path);
  }

  tags.normalized.title = normalize(arena, tags.title);
  tags.normalized.artist = normalize(arena, tags.artist);
  tags.normalized.album.title = normalize(arena, tags.album.title);
  tags.normalized.album.artist = normalize(arena, tags.album.artist);
  
  tags.cover.texture = -1;

  const char *cover_names[] = {
    "cover.png",
    "album.png",
    "front.png",
    "cover.jpg",
    "cover.jpeg",
    "album.jpg",
    "front.jpg",
    NULL,
  };

  char *path_copy = a_malloc(arena, strlen(path) + 1);
  strcpy(path_copy, path);
  char *parent = dirname(path_copy);
  for (const char **name = cover_names; *name; name++) {
    const char *cover_path = a_sprintf(arena, "%s/%s", parent, *name);
    if (path_is_file(cover_path)) {
      tags.cover.exists = true;
      tags.cover.offset = 0;
      tags.cover.size = path_filesize(cover_path);
      char *cover_path_owned = a_malloc(arena, strlen(cover_path) + 1);
      memcpy(cover_path_owned, cover_path, strlen(cover_path) + 1);
      tags.cover.path = cover_path_owned;
      tags.cover.ext = strrchr(cover_path_owned, '.');
      if (tags.cover.ext == NULL) {
	if (memcmp(magic, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 3) == 0) tags.cover.ext = ".png";
	else if (memcmp(magic, "\xFF\xD8\xFF", 4) == 0) tags.cover.ext = ".jpg";
	else tags.cover.ext = "uh-oh!";
      }
      break;
    }
  }
  
  return tags;
}
