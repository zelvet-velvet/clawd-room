# README: the office. Three sessions walk in; one works, one thinks, one waits for an answer, then one finishes and cheers.
theme office
title office
terminal 168 50
run 400 16 2
ev 0.0 s00 SessionStart web-shop -
ev 0.5 s01 SessionStart api-server -
ev 1.0 s02 SessionStart docs-site -
ev 8.0 s00 UserPromptSubmit web-shop -
ev 8.6 s00 PreToolUse web-shop Bash
ev 9.0 s01 UserPromptSubmit api-server -
ev 10.0 s02 UserPromptSubmit docs-site -
ev 10.4 s02 PreToolUse docs-site Edit
ev 11.5 s01 Notification api-server -
ev 15.0 s00 Stop web-shop -
gif 8.0 19.0 2
