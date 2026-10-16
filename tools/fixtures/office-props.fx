theme office
title room-office-props
terminal 120 40
time 1000.0 3.4
scale 1
session s00 name=clawd-room created=900 state=idle tool=Bash px=10 py=15 
session s01 name=park created=910 state=idle tool=Edit px=40 py=15 cup=1010
session s02 name=api created=920 state=idle tool=mcp__x__y px=70 py=15 chat_with=s03 stage=talk chat_kind=0
session s03 name=web created=930 state=idle tool=Read px=100 py=15 chat_with=s02 stage=talk chat_kind=2
session s04 name=docs created=940 state=idle tool=Foo px=10 py=55 emote=jump@990
session s05 name=infra created=950 state=idle tool=Task px=40 py=55 react=1010
session s06 name=tests created=960 state=idle tool=Bash px=70 py=55 peek=1
session s07 name=notes created=970 state=idle tool=Edit px=100 py=55 
particle 30.5 20 0 0 0.9 240 96 144 1
particle 31 22 0 0 0.9 250 210 70 0
particle 60 22 0 0 0.1 250 210 70 0
guest g1 name=visitor px=50 py=45 face=-1
guest g2 name=wanderer px=70.5 py=46 face=1 moving=1 walk_phase=4 chat_with=g1 stage=talk chat_kind=1
