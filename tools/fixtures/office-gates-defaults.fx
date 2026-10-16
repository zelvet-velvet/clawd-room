theme office
title room-office-gates-defaults
terminal 110 32
time 1000.0 3.4
scale 1
neighbor left office
neighbor right coffee
neighbor up park
neighbor down office
session s00 name=clawd-room created=900 state=work tool=Bash px=32 py=31
session s01 name=park created=910 state=think tool=Edit px=68 py=31 hold=1000.4
