summon minecraft:marker ~ ~ ~ {Tags:["mirror_waypoint"],hexbladeid:0}
execute at @s as @e[tag=mirror_waypoint,limit=1,sort=nearest,distance=..5] run forceload add ~ ~
scoreboard players operation @e[tag=mirror_waypoint,limit=1,sort=nearest,distance=..5] hexhold = @s hexhold
tellraw @p {"text":"Your mirror will remember this location...","color":"dark_aqua"}