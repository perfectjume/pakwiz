#Summon AOE cloud with correct ID and tags
execute as @p[tag=hexblade] at @s run summon minecraft:area_effect_cloud ~ ~1 ~ {Potion:"minecraft:water",Duration:8,Radius:1.2f,RadiusOnUse:-5f,WaitTime:0,ReapplicationDelay:0,Effects:[{Id:21,Duration:0,Amplifier:0b}],Tags:["cloud","health_boost"]}
tellraw @p[tag=hexblade,distance=0.1..10] {"text":"Stealing health boost effect","color":"green"}
#Copy duration and amplifier
execute as @s store result entity @e[tag=cloud,tag=health_boost,sort=nearest,limit=1] Effects[0].Duration int 1 run data get entity @s ActiveEffects[{Id:21}].Duration
execute as @s store result entity @e[tag=cloud,tag=health_boost,sort=nearest,limit=1] Effects[0].Amplifier byte 1 run data get entity @s ActiveEffects[{Id:21}].Amplifier

#Remove effect from target
effect clear @s minecraft:health_boost