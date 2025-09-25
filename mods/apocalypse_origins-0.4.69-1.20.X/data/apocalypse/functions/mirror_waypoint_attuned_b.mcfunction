summon minecraft:marker ~ ~ ~ {Tags:["mirror_waypoint_b"],hexbladeid:0}
execute at @s as @e[tag=mirror_waypoint_b,limit=1,sort=nearest,distance=..5] run forceload add ~ ~
scoreboard players operation @e[tag=mirror_waypoint_b,limit=1,sort=nearest,distance=..5] hexhold = @s hexhold
tellraw @p {"text":"Attunement shard: The shard has been attuned to this location. Ready to travel to the other end.","color":"blue"}