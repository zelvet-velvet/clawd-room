SHELL := /bin/bash
CFLAGS ?= -O2 -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
OBJS = main.o canvas.o sprites.o clawd.o term.o

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
	@echo "C port matches Python"
.PHONY: test
