# generated: coffee-gates-busy
theme coffee
title cafe2
terminal 120 40
neighbor right office
run 1600 16 40
ev 0.0 s00 SessionStart alpha -
ev 0.1 s01 SessionStart beta -
ev 0.2 s02 SessionStart gamma -
ev 0.3 s03 SessionStart delta -
ev 0.4 s04 SessionStart eps -
ev 5.0 s00 PreToolUse alpha Bash
ev 9.0 s00 PostToolUse alpha Bash
ev 14.0 s01 Stop beta -
ev 20.0 s02 Notification gamma -
ev 40.0 s03 SessionEnd delta -
msg 15.0 id-right walk v7 visitor left 0 100
