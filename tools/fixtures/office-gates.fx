theme office
title room-office-gates
terminal 110 32
time 1000.0 3.4
scale 1
peer beta right
peer gamma up
peer delta -
neighbor left coffee
neighbor right park
neighbor up office
neighbor down park
linkpos left 40.5
linkpos right 33
linkpos up 70
linkpos down 25.5
gate left 1.0
gate right 1.6
gate up 2.0
gate down 1.3
session s00 name=clawd-room created=900 state=work tool=Bash px=14 py=31
session s01 name=park created=910 state=think tool=Edit px=50 py=31 hold=1000.4
session s02 name=api created=920 state=wait tool=mcp__x__y px=86 py=31
