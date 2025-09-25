summon minecraft:marker ~ ~ ~ {Tags:["mirror_waypoint_a"],hexbladeid:0}
execute at @s as @e[tag=mirror_waypoint_a,limit=1,sort=nearest,distance=..5] run forceload add ~ ~
scoreboard players operation @e[tag=mirror_waypoint_a,limit=1,sort=nearest,distance=..5] hexhold = @s hexhold
tellraw @p {"text":"Attunement shard: The shard has been attuned to this location.","color":"gold"}