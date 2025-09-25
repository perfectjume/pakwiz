tag @s add current
execute as @s at @s as @e[tag=mirror_waypoint_a,sort=nearest] at @s if score @s hexhold = @p[tag=current] hexbladeid run forceload remove ~ ~
execute as @s at @s as @e[tag=mirror_waypoint_a,sort=nearest] at @s if score @s hexhold = @p[tag=current] hexbladeid run kill @e[tag=mirror_activator,limit=1,sort=nearest,distance=..5]
execute as @s at @s as @e[tag=mirror_waypoint_a,sort=nearest] at @s if score @s hexhold = @p[tag=current] hexbladeid run kill @e[tag=mirror_waypoint_a,limit=1,sort=nearest,distance=..5]
execute as @s at @s as @e[tag=mirror_waypoint_b,sort=nearest] at @s if score @s hexhold = @p[tag=current] hexbladeid run forceload remove ~ ~
execute as @s at @s as @e[tag=mirror_waypoint_b,sort=nearest] at @s if score @s hexhold = @p[tag=current] hexbladeid run kill @e[tag=mirror_activator,limit=1,sort=nearest,distance=..5]
execute as @s at @s as @e[tag=mirror_waypoint_b,sort=nearest] at @s if score @s hexhold = @p[tag=current] hexbladeid run kill @e[tag=mirror_waypoint_b,limit=1,sort=nearest,distance=..5]
tellraw @p {"text":"Attunement shard: Removed all existing links.","color":"gold"}
tag @s remove current