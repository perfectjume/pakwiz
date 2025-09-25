summon minecraft:marker ~ ~ ~ {Tags:["hexblade_waypoint"],hexbladeid:0}
execute at @s as @e[tag=hexblade_waypoint,limit=1,sort=nearest,distance=..5] run forceload add ~ ~
scoreboard players operation @e[tag=hexblade_waypoint,limit=1,sort=nearest,distance=..5] hexhold = @s hexhold
tellraw @p {"text":"I will be back...","color":"red"}