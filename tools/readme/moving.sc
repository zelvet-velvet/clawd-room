# README: moving a Clawd to the next room (the m key): it packs its desk into a box and carries it out through the door.
theme office
title office
terminal 168 50
neighbor right coffee
linkpos right 33
run 480 16 3
ev 0.0 s00 SessionStart web-shop -
ev 0.5 s01 SessionStart api-server -
ev 1.0 s02 SessionStart docs-site -
ev 8.0 s01 UserPromptSubmit api-server -
ev 8.4 s01 PreToolUse api-server Bash
key 10.0 m s01 right
gif 9.5 24.0 3
