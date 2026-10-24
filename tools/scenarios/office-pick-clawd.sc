# Tab picks a Clawd: yellow corners close in around it and the room dims (fade in 0.25 s), a second Tab glides to the next, Esc lets go of it: they fade out (0.4 s); without Esc they fade 2.5 s after the last Tab
theme office
title pick
terminal 140 50
run 176 16 4
ev 0.0 a SessionStart alpha -
ev 0.1 b SessionStart beta -
ev 0.2 c SessionStart gamma -
ev 0.3 d SessionStart delta -
ev 1.0 a UserPromptSubmit alpha -
ev 1.2 c UserPromptSubmit gamma -
key 3.0 tab a
key 4.0 tab c
key 5.0 esc
