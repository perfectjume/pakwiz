tag @s add current
execute as @s at @s as @e[tag=mirror_waypoint,sort=nearest] at @s if score @s hexhold = @p[tag=current] hexbladeid run tp @p[tag=current] ~ ~ ~
execute at @s as @e[tag=mirror_waypoint,limit=1,sort=nearest,distance=..5] run forceload remove ~ ~
execute at @s as @e[tag=mirror_waypoint,limit=1,sort=nearest,distance=..5] run kill @e[tag=mirror_waypoint,limit=1,sort=nearest,distance=..5]
tag @s remove current