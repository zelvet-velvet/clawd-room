# four sessions in the office over 40 s: start, work, think, wait, done, a session that ends
theme office
title day
terminal 110 32
run 640 16 8
ev 0.0 a SessionStart alpha -
ev 0.1 b SessionStart beta -
ev 0.2 c SessionStart gamma -
ev 0.3 d SessionStart delta -
ev 2.0 a UserPromptSubmit alpha -
ev 3.0 a PreToolUse alpha Bash
ev 5.0 a PostToolUse alpha Bash
ev 6.0 a PreToolUse alpha Edit
ev 8.0 b UserPromptSubmit beta -
ev 9.0 b Notification beta -
ev 12.0 a Stop alpha -
ev 20.0 b Stop beta -
ev 30.0 d SessionEnd delta -
