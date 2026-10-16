SHELL := /bin/bash
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -Wno-format-truncation
OBJS = main.o canvas.o sprites.o clawd.o term.o theme.o draw.o fixture.o rng.o room.o script.o tail.o ui.o app.o json.o link.o win.o

clawd-room: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lm

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) clawd-room
.PHONY: clean

# the C port must draw exactly what room_tui.py draws: compare frame hashes and terminal strings
test: clawd-room
	./clawd-room --golden | diff - <(python3 ../tools/golden_clawd.py)
	./clawd-room --golden --code | diff - <(python3 ../tools/golden_clawd.py --code)
	./clawd-room --golden-term | diff - <(python3 ../tools/golden_clawd.py --term)
	./clawd-room --fixture ../tools/fixtures/*.fx | diff - <(python3 ../tools/golden_fixture.py ../tools/fixtures/*.fx)
	./clawd-room --golden-rng | diff - <(python3 ../tools/golden_rng.py)
	./clawd-room --script ../tools/scenarios/*.sc | diff - <(python3 ../tools/golden_room.py ../tools/scenarios/*.sc)
	./clawd-room --golden-ui ../tools/fixtures/events-odd.jsonl | diff - <(python3 ../tools/golden_ui.py ../tools/fixtures/events-odd.jsonl)
	python3 ../tools/compare_dump.py
	./clawd-room --golden-win | diff - <(python3 ../tools/golden_win.py)
	python3 ../tools/gen_ps.py --check
	@echo "C port matches Python"
.PHONY: test

# two linked rooms (C and Python in every mix): send a Clawd with g, check the messages arrive (about 15 s)
linksmoke: clawd-room
	python3 ../tools/link_smoke.py
.PHONY: linksmoke

# drives the real program in a pty (about 30 s): keys, new events, menu, quitting, terminal restored
smoke: clawd-room
	python3 ../tools/pty_smoke.py c
.PHONY: smoke

# real Windows Terminal windows (your own alpha / beta): sizing and where the gates go, for C and Python (about 1 minute each)
winsmoke: clawd-room
	python3 ../tools/win_smoke.py python 10
	python3 ../tools/win_smoke.py c 10
.PHONY: winsmoke
