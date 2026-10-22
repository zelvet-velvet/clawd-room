# windows come next to the office while it runs: the stairs, a side door and the well appear (anims/gate-*.txt; the test runs it
# with other files too, and without them)
theme office
title office
terminal 160 50
run 112 16 4
ev 0.0 s00 SessionStart alpha -
ev 0.1 s01 SessionStart beta -
appear 2.0 up office
appear 2.5 right coffee
appear 3.0 down office
