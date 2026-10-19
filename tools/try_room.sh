#!/bin/sh
# One room of clawd-room.sh --try: runs the room; when any of the --try rooms quits (the STOP file appears) this one stops
# too, and when this one quits it writes STOP so the others follow.
#     try_room.sh BIN STOP ARGS...
bin=$1 stop=$2
shift 2
( while [ ! -e "$stop" ]; do sleep 0.3; done; pkill -TERM -P $$ ) &
"$bin" "$@"
rc=$?
[ -e "$stop" ] || echo "$* exit $rc" > "$stop" # (the stop file says which room ended the --try, and how)
