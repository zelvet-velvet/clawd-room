# README: a gate being built from its own pieces (the construct theme: run with CLAWD_GATE_THEME=construct): the door.
theme office
title office
terminal 168 50
run 260 16 2
ev 0.0 s00 SessionStart web-shop -
ev 0.5 s01 SessionStart api-server -
appear 8.0 right office
gif 7.8 10.6 2 110 30 58 58 8
