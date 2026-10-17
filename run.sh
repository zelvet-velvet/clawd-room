#!/usr/bin/env bash
# Start the room window (WSLg). Needs no Claude Code sessions to be running.
cd "$(dirname "$0")" && exec .venv/bin/python room.py "$@"
