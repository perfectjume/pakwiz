tag @s add current
execute as @s at @s as @e[tag=mirror_waypoint_b,sort=nearest] at @s if score @s hexhold = @p[tag=current] hexbladeid run execute at @e[tag=mirror_waypoint_b,distance=..10] as @e[distance=..2] at @s run function apocalypse:attuned_teleportation_b
execute as @s at @s as @e[tag=mirror_waypoint_b,sort=nearest] at @s if score @s hexhold = @p[tag=current] hexbladeid run execute as @s at @e[tag=mirror_waypoint_b,distance=..10] run summon area_effect_cloud ~ ~ ~ {Particle:"minecraft:dripping_water",Radius:2f,Duration:300,RadiusPerTick:-0.1f}
tag @s remove current