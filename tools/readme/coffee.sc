# README: the coffee shop: the barista works behind the bar, the others sit at the tables.
theme coffee
title coffee
terminal 168 50
run 300 16 2
ev 0.0 s00 SessionStart web-shop -
ev 0.5 s01 SessionStart api-server -
ev 1.0 s02 SessionStart docs-site -
ev 8.0 s00 UserPromptSubmit web-shop -
ev 8.4 s00 PreToolUse web-shop Bash
ev 9.0 s02 UserPromptSubmit docs-site -
ev 9.4 s02 PreToolUse docs-site Edit
gif 8.0 16.0 2
