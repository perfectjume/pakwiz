execute as @s run scoreboard players operation @s hexhold = @e[tag=mirror_waypoint_b,distance=..3] hexhold
execute as @s run function apocalypse:find_pair_b
execute as @s[type=!minecraft:marker,tag=!refractor] run scoreboard players reset @s[type=!minecraft:marker,tag=!refractor] hexhold