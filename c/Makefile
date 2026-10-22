SHELL := /bin/bash
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -Wno-format-truncation
# a*b+c in one rounding (FMA) on both platforms, so the snapshots match: clang on Apple silicon fuses by default; on x86-64
# gcc needs -mfma (any CPU since 2013) and -ffp-contract=fast (-std=c11 turns fusing off). A CPU without FMA builds without it.
ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
  ifneq ($(shell grep -m1 -wo fma /proc/cpuinfo 2>/dev/null),)
    CFLAGS += -mfma -ffp-contract=fast
  endif
else
  CFLAGS += -ffp-contract=on
endif
OBJS = main.o canvas.o sprites.o clawd.o term.o theme.o draw.o fixture.o rng.o room.o script.o tail.o ui.o app.o json.o link.o win.o actions.o art.o camera.o persp.o scene.o gallery.o place.o editor.o anim.o parts.o construct.o settings.o

clawd-room: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lm

%.o: %.c $(wildcard *.h *.inc) # (editor.c includes stage.inc)
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

# two linked rooms in ptys: m + arrow moves a Clawd, the other room claims it (about 15 s)
movesmoke: clawd-room
	python3 ../tools/move_smoke.py
.PHONY: movesmoke

# linked rooms keep their share of the sessions: a room opens, one freezes, one closes (about 25 s)
splitsmoke: clawd-room
	python3 ../tools/split_smoke.py
.PHONY: splitsmoke

# real Windows Terminal windows (your own alpha / beta): sizing and where the gates go (about 1 minute)
winsmoke: clawd-room
	python3 ../tools/win_smoke.py 10
.PHONY: winsmoke

# every room x gates x empty / some / crowded x three sizes, as contact sheets in ../gallery (and the overlap check)
gallery: clawd-room
	mkdir -p ../gallery && CLAWD_SPRITES=../sprites ./clawd-room --gallery ../gallery
.PHONY: gallery
