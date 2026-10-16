# a room with no sessions of its own: a Clawd comes in through the left door, wanders and goes home again
theme office
title empty
terminal 110 32
neighbor left coffee
run 1280 16 40
msg 2.0 id-left walk s1 visitor right 0 500
msg 10.0 id-left recall s1 - right 0 0
