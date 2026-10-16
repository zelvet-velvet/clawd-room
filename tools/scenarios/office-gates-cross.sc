# generated: office-gates-cross
theme office
title cross
terminal 120 40
neighbor left coffee
neighbor right park
neighbor up office
neighbor down park
run 2400 16 60
ev 0.0 s00 SessionStart alpha -
ev 0.1 s01 SessionStart beta -
ev 0.2 s02 SessionStart gamma -
ev 0.3 s03 SessionStart delta -
ev 0.4 s04 SessionStart eps -
key 8.0 g
key 20.0 g
key 40.0 g
msg 12.0 id-left walk vis1 vizzy right 0 500
msg 30.0 id-up walk vis2 upper down 0 300
msg 45.0 id-right recall s00 - right 0 0
ev 60.0 s01 UserPromptSubmit beta -
gone 100.0 id-down
