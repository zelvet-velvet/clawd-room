theme coffee
title room-coffee-visit
terminal 120 40
time 1000.0 3.4
scale 1
session s00 name=clawd-room created=900 state=idle tool=Bash px=10 py=39 chat_with=s01 stage=talk chat_kind=6 part=host
session s01 name=park created=910 state=idle tool=Edit px=40 py=39 chat_with=s00 stage=talk chat_kind=6 part=visitor
session s02 name=api created=920 state=idle tool=mcp__x__y px=70 py=39 
session s03 name=web created=930 state=idle tool=Read px=100 py=39 cup=1100
