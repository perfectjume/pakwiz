tag @s add hexpectate
execute as @s at @s as @a[distance=0.01..] if score @s hexhold = @p[tag=hexpectate] hexbladeid run tag @s add hextarget
execute as @s at @s run spectate @p[tag=hextarget]
tag @s remove hexpectate
tag @a[distance=..10] remove hextarget