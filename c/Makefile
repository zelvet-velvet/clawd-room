SHELL := /bin/bash
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -Wno-format-truncation
OBJS = main.o canvas.o sprites.o clawd.o term.o theme.o draw.o fixture.o rng.o room.o script.o

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
	@echo "C port matches Python"
.PHONY: test
