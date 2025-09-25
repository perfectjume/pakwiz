tag @s add hexpectate
execute as @a[distance=0.01..] if score @s hexhold = @p[tag=hexpectate] hexbladeid run tag @p[tag=hexpectate] add fail
execute as @s[tag=fail] run gamemode spectator
execute as @s[tag=fail] run resource change @s apocalypse:hexblade/hexblade_resource_souls -10
execute as @s[tag=fail] run function apocalypse:hexblade_spectate
execute as @s[tag=fail] run power grant @s apocalypse:hexblade/hexblade_spectating_controls hexblade_bars
execute as @s[tag=!fail] run function apocalypse:hexblade_spectate_no
execute as @s[tag=!fail] run gamemode survival
execute as @s[tag=!fail] run tellraw @p {"text":"No ally was found!","color":"dark_red"}
tag @s remove hexpectate
tag @s remove fail