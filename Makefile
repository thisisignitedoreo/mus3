CC := gcc
AR := ar
A_LDFLAGS := -lm -lX11 -lXrandr -lGL
CFLAGS := -Wall -Wextra -Werror -pedantic -Wno-missing-field-initializers -Wno-unused-parameter -Wno-missing-braces -std=c99 -O3 -fPIE -fpic -flto $(A_CFLAGS)
CFLAGS += -Iext/raylib/src -Iext/cs/include -Iext/miniaudio -Iext/raytext -Ibuild -Iinclude -D_GNU_SOURCE
LDFLAGS := -L/usr/lib/ -Lext/raylib/src -Lext/cs/build -lraylib -lcs $(A_LDFLAGS)

RAYLIB_ARGS :=

OBJS = $(patsubst src/%.c,build/%.o,$(wildcard src/*.c)) build/miniaudio.o build/raytext.o
HEADERS = $(wildcard include/*.h)

ifndef RELEASE
CFLAGS += -g
LDFLAGS := -lasan $(LDFLAGS)
endif

ASSETS := font.ttf about.txt next.png previous.png repeat_linear.png repeat_album.png repeat_one.png repeat_shuffle.png play.png pause.png arrow_up.png arrow_down.png check.png
ASSETS := $(patsubst %,assets/%,$(ASSETS))

VERSION := v0.0.1

ifndef RELEASE
VERSION := $(VERSION)-indev
endif

ifdef MPRIS
MPRIS_DEFINED := 1
CFLAGS += -Iext/systemd/src/systemd/ -Iext/systemd/src/libsystemd/sd-bus/
OBJS += build/libsystemd.a build/backend-mpris.o
LDFALGS += -Lbuild -lsystemd
else
MPRIS_DEFINED := 0
OBJS += build/backend-null.o
endif

all: build/mus3
.PHONY: clean all .FORCE
.FORCE:

clean:
	rm -fr build
	rm -fr ext/cs/build
	rm -fr ext/systemd/build
	rm -f assets/about.txt
	rm -f ext/raylib/src/libraylib.a
	rm -f ext/raylib/src/*.o

build:
	mkdir -p build

ext/cs/build/libcs.a:
	cd ext/cs; make CC="$(CC)" AR="$(AR)"

ext/raylib/src/libraylib.a:
	cd ext/raylib/src; make $(RAYLIB_ARGS) PLATFORM=PLATFORM_DESKTOP_GLFW GLFW_LINUX_ENABLE_WAYLAND=TRUE CC="$(CC)" AR="$(AR)" RAYLIB_MODULE_AUDIO=FALSE RAYLIB_MODULE_MODELS=FALSE

build/%.o: src/%.c $(HEADERS) build/assets.h | build
	$(CC) $(CFLAGS) -c -o $@ $<

build/backend-%.o: src/media-backends/%.c $(HEADERS) | build
	$(CC) $(CFLAGS) -c -o $@ $<

build/miniaudio.o: ext/miniaudio/miniaudio.c | build
	$(CC) -DMA_DEBUG_OUTPUT -c -o $@ $<

build/libsystemd.a: ext/systemd/
	mkdir -p ext/systemd/build
	CC=$(CC) meson setup ext/systemd/build ext/systemd -Dstatic-libsystemd=true
	cd ext/systemd/build; ninja
	cp ext/systemd/build/libsystemd.a build/

build/raytext.o: ext/raytext/raytext.c
	$(CC) -Iext/raylib/src -Iext/raytext -c -o $@ $^

build/bundler: bundler.c
	$(CC) -o $@ $<

assets/about.txt: assets/about.txt.m4
	m4 -DMUS3_VERSION=$(VERSION) -DUSING_MPRIS=$(MPRIS_DEFINED) assets/about.txt.m4 > assets/about.txt

build/assets.h: build/bundler $(ASSETS)
	build/bundler -o build/assets.h \
		--name font assets/font.ttf \
		--name about assets/about.txt \
		--name play assets/play.png \
		--name pause assets/pause.png \
		--name next assets/next.png \
		--name previous assets/previous.png \
		--name repeat_linear assets/repeat_linear.png \
		--name repeat_album assets/repeat_album.png \
		--name repeat_one assets/repeat_one.png \
		--name repeat_shuffle assets/repeat_shuffle.png \
		--name arrow_up assets/arrow_up.png \
		--name arrow_down assets/arrow_down.png \
		--name check assets/check.png

build/mus3: build build/assets.h $(OBJS) $(HEADERS) ext/cs/build/libcs.a ext/raylib/src/libraylib.a
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
