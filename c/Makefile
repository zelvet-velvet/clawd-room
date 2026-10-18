SHELL := /bin/bash
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -Wno-format-truncation
OBJS = main.o canvas.o sprites.o clawd.o term.o theme.o draw.o fixture.o rng.o room.o script.o tail.o ui.o app.o json.o link.o win.o actions.o art.o camera.o persp.o scene.o gallery.o place.o editor.o

clawd-room: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lm

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) clawd-room
.PHONY: clean

# the program against the snapshots in tests/expected (see ../tools/test.sh)
test: clawd-room
	../tools/test.sh
.PHONY: test

# two linked rooms in ptys: send a Clawd with g, check the messages arrive (about 10 s)
linksmoke: clawd-room
	python3 ../tools/link_smoke.py
.PHONY: linksmoke

# drives the real program in a pty (about 30 s): keys, new events, menu, quitting, terminal restored
smoke: clawd-room
	python3 ../tools/pty_smoke.py
.PHONY: smoke

# real Windows Terminal windows (your own alpha / beta): sizing and where the gates go (about 1 minute)
winsmoke: clawd-room
	python3 ../tools/win_smoke.py 10
.PHONY: winsmoke

# every room x gates x empty / some / crowded x three sizes, as contact sheets in ../gallery (and the overlap check)
gallery: clawd-room
	mkdir -p ../gallery && CLAWD_SPRITES=../sprites ./clawd-room --gallery ../gallery
.PHONY: gallery
