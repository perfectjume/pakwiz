tag @e[distance=..2,type=!ender_dragon,type=!wither] add target_tp
scoreboard players operation #target_tp hexhold = @s hexhold
playsound minecraft:block.beacon.ambient master @p[distance=..10] ~ ~ ~ 1 2
execute as @e[tag = mirror_waypoint_b] at @s if score @s hexhold = #target_tp hexhold run tp @e[tag=target_tp,type=!minecraft:marker,type=!minecraft:armor_stand] ~ ~ ~
execute as @e[tag = mirror_waypoint_b] at @s if score @s hexhold = #target_tp hexhold run playsound minecraft:block.beacon.power_select master @p[distance=..10] ~ ~ ~ 1 2
tag @e[tag=target_tp] remove target_tp