execute as @p[tag=refractor, sort=nearest, limit=1, distance=..5] run power revoke @s apocalypse:refractor/refractor_reflect_item
execute as @p[tag=refractor, sort=nearest, limit=1, predicate=!apocalypse:dead_predicate, distance=..5] at @s run function apocalypse:mirror_give
execute as @p[tag=refractor, sort=nearest, limit=1, predicate=!apocalypse:dead_predicate, distance=..5] at @s run tellraw @s {"text":"Your mirror returns...","color":"dark_aqua"}
execute as @p[tag=refractor, sort=nearest, limit=1, predicate=!apocalypse:dead_predicate, distance=..5] at @s run playsound minecraft:block.amethyst_cluster.break master @a[distance=..5] ~ ~ ~ 10 2
kill @s